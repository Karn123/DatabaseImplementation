//Implemented by Lai ZhengMin & Xu Jing

//������¼=sum(����),��ı���ǴӴ�С��(block->preNum=block->getNum()+1)
#include "RecordManager.h"
#include "BPlusTree.h"
#include "ConstValue.h"
#include<iostream>
#include <iomanip>
using std::cout;

RecordManager::RecordManager(CatalogManager *cm, BufferManager *bm, string dbname) :catalog_m_(cm), buffer_m_(bm), db_name_(dbname) {}

RecordManager::~RecordManager(void) {}

string RecordManager::intToString(int x)
{
	char t[1000];
	string s;
	sprintf_s(t, "%d", x);
	s = t;
	return s;
}

string RecordManager::floatToString(float x)
{
	char t[1000];
	string s;
	sprintf_s(t, "%f", x);
	s = t;
	return s;
}

void RecordManager::Insert(SQLInsert& st, bool&flag)
{
	string tb_name = st.get_tb_name();
	//��������¼�ж��ٸ�key value
	int values_size = st.GetValues().size();
	//���ݱ�������Ŀ¼�ļ����õ����ݿ��еı�
	Table *tb = catalog_m_->GetDB(db_name_)->GetTable(tb_name);
	if (tb == NULL) throw TableNotExistException();

	//һ�飨4K��ͷ12 bytes����װ���ٸ���¼��tuple��
	int max_count = (4096 - 12) / (tb->get_record_length());

	vector<TKey> tkey_values;

	int primary_key_index = -1;

	//���������¼�����Ƿ��������������������ͺͼ�ֵ�浽tkey_values��
	for (int i = 0; i < values_size; i++)
	{
		int value_type = st.GetValues()[i].data_type;
		string value = st.GetValues()[i].value;
		int length = tb->GetAttributes()[i].get_length();

		TKey tmp(value_type, length);
		tmp.ReadValue(value.c_str());

		tkey_values.push_back(tmp);
		if (tb->GetAttributes()[i].get_attr_type() == 1)
			primary_key_index = i;
	}
	//���������
	if (primary_key_index != -1)
	{
		//���������
		if (tb->GetIndexNum() != 0)
		{
			BPlusTree tree(tb->GetIndex(0), buffer_m_, catalog_m_, db_name_);
			if (tree.get_value(tkey_values[primary_key_index]) != -1)
				throw PrimaryKeyConflictException();
		}
		//������
		else
		{
			//�õ��ñ����ʼ��� 
			int block_num = tb->get_first_block_num();
			//�������еĿ飬������������Ƿ�ᷢ��������ͻ
			for (int i = 0; i < tb->get_block_count(); i++)
			{
				//�õ��ÿ�Ŷ�Ӧ�Ŀ���Ϣ
				BlockInfo *bp = GetBlockInfo(tb, block_num);
				for (int j = 0; j < bp->GetRecordCount(); j++)
				{
					//�õ����ڵĵ�j����¼
					vector<TKey> tuple = GetRecord(tb, block_num, j);
					//�����������ֵ������ͻ
					if (tuple[primary_key_index] == tkey_values[primary_key_index])
						throw PrimaryKeyConflictException();
				}
				block_num = bp->GetNextBlockNum();
			}
		}
	}
	char *content;
	//���ÿ����ʼ���
	int use_block = tb->get_first_block_num();
	//���������ʼ���
	int first_rubbish_block = tb->get_first_rubbish_num();
	//��¼��һ��ʹ�õĿ�
	int last_use_block;
	int blocknum, offset;
	//�޿��ÿ�
	while (use_block != -1)
	{
		last_use_block = use_block;
		BlockInfo *bp = GetBlockInfo(tb, use_block);
		//������
		if (bp->GetRecordCount() == max_count)
		{
			use_block = bp->GetNextBlockNum();
			continue;
		}
		//δ��������recordβ�������¼
		content = bp->GetContentAdress() + bp->GetRecordCount() * tb->get_record_length();
		//����һ��tuple��Ҳ���Ǵӿ�Ŀ���λ�ò���һ��tuple
		for (auto iter = tkey_values.begin(); iter != tkey_values.end(); iter++)
		{
			//���Ƹ�content
			memcpy(content, iter->get_key(), iter->get_length());
			content += iter->get_length();
		}
		bp->SetRecordCount(1 + bp->GetRecordCount());
		//��¼����Ŀ��
		blocknum = use_block;
		//��¼����ƫ����
		offset = bp->GetRecordCount() - 1;
		//���¿���Ϣ
		buffer_m_->WriteBlock(bp);

		//���������
		if (tb->GetIndexNum() != 0)
		{
			BPlusTree tree(tb->GetIndex(0), buffer_m_, catalog_m_, db_name_);
			for (auto i = 0; i < tb->GetAttributes().size(); i++)
			{
				if (tb->GetIndex(0)->get_attr_name() == tb->GetIndex(i)->get_attr_name())
				{
					tree.add(tkey_values[i], blocknum, offset);
					break;
				}
			}
		}
		//�����º�Ľ��д�ش���
		buffer_m_->WriteToDisk();
		//��Ŀ¼��Ϣд�ش���
		catalog_m_->WriteArchiveFile();
		if (flag)
			cout << "����ɹ���" << endl;
		return;
	}
	//����޿��ÿ鵫�������飨�Ѿ������յĿտ飩����嵽��������
	if (first_rubbish_block != -1)
	{
		//�õ���һ��������
		BlockInfo *bp = GetBlockInfo(tb, first_rubbish_block);
		content = bp->GetContentAdress();
		//����record��content��
		for (auto iter = tkey_values.begin(); iter != tkey_values.end(); iter++)
		{
			memcpy(content, iter->get_key(), iter->get_length());
			content += iter->get_length();
		}
		//������ļ�¼Ϊ1
		bp->SetRecordCount(1);
		//�õ�����whileѭ��֮ǰ��block������nextBlockNumΪ-1
		BlockInfo *last_use_block_p = GetBlockInfo(tb, last_use_block);
		//��first_rubbish_block�������ĺ���
		last_use_block_p->SetNextBlockNum(first_rubbish_block);
		//���������ָ�����һλ
		tb->set_first_rubbish_num(bp->GetNextBlockNum());
		//�ÿ��ǰһ��Ϊlast_use_block
		bp->SetPrevBlockNum(last_use_block);

		//�����ÿ�ĺ����޿��ÿ�
		bp->SetNextBlockNum(-1);
		//���²����Ŀ��Ϊfirst_rubbish_block
		blocknum = first_rubbish_block;
		//����ƫ����Ϊ0����Ϊ���Ǹÿ�ĵ�һ����¼
		offset = 0;
		//��bp��last_use_block_p��Ϊ��飬�ȴ����д�ش���
		buffer_m_->WriteBlock(bp);
		buffer_m_->WriteBlock(last_use_block_p);
	}
	else//�����ǰ���޿��ÿ�Ҳ�������鹩���룬��Ҫ����һ���¿�
	{
		int next_block = tb->get_first_block_num();
		//������ǵ�һ�β���
		if (next_block != -1)
		{
			BlockInfo *up = GetBlockInfo(tb, tb->get_first_block_num());
			//������֮ǰ�Ŀ�ı��Ϊblock_count�����Լ���1��
			up->SetPrevBlockNum(tb->get_block_count());
			buffer_m_->WriteBlock(up);
		}
		//���õ�һ�����ÿ�ı��
		tb->set_first_block_num(tb->get_block_count());
		//����һ���¿�
		BlockInfo *bp = GetBlockInfo(tb, tb->get_first_block_num());
		//��ǰ���޿�
		bp->SetPrevBlockNum(-1);
		//��next_block���������棬prev_numҪ���Լ���num��
		bp->SetNextBlockNum(next_block);
		//����¼��1���Ѵ����¼�浽bp����
		bp->SetRecordCount(1);
		content = bp->GetContentAdress();
		for (auto iter = tkey_values.begin(); iter != tkey_values.end(); iter++)
		{
			memcpy(content, iter->get_key(), iter->get_length());
			content += iter->get_length();
		}
		//���²���Ŀ��
		blocknum = tb->get_block_count();
		//����ƫ����Ϊ0
		offset = 0;
		//��Ϊ���
		buffer_m_->WriteBlock(bp);
		//����Ŀ�����1
		tb->IncreaseBlockCount();
	}
	//�����index,�Ѽ�¼�嵽index��
	if (tb->GetIndexNum() != 0)
	{
		BPlusTree tree(tb->GetIndex(0), buffer_m_, catalog_m_, db_name_);
		for (auto i = 0; i < tb->GetAttributes().size(); i++)
		{
			if (tb->GetIndex(0)->get_attr_name() == tb->GetIndex(i)->get_attr_name())
			{
				tree.add(tkey_values[i], blocknum, offset);
				break;
			}
		}
	}
	//��bufferд�ش���
	buffer_m_->WriteToDisk();
	catalog_m_->WriteArchiveFile();
	//�����Ƿ�Ҫ�������ɹ���Ϣ
	if (flag)
		cout << "����ɹ���" << endl;
}
//Select ���� ��֧������������ͬ������A=����B ��ѯ��
vector<vector<TKey>> RecordManager::Select(SQLSelect& st)
{
	string searchType = "��ͨ��ѯ";
	Table *tb = catalog_m_->GetDB(db_name_)->GetTable(st.get_tb_name());

	//ɸѡ���ֶε��±꼯��
	vector<int> attribute_loc;
	vector<vector<TKey> > tuples;
	vector<vector<TKey>>result;
	bool isAggregateFunction = false;
	//�ۼ��������õ�����
	vector<string> aggregateFunctionAttributes;
	vector<string> check = st.get_select_attribute();
	if (check.size() > 0)
	{
		if ((check[0].find("count(") != string::npos) || (check[0].find("avg(") != string::npos)
			|| (check[0].find("max(") != string::npos) || (check[0].find("min(") != string::npos))
		{
			isAggregateFunction = true;
			for (int i = 0; i < st.get_select_attribute().size(); i++)
			{
				string s = st.get_select_attribute()[i];
				int pos_1 = s.find("(");
				int pos_2 = s.find(")");
				aggregateFunctionAttributes.push_back(s.substr(pos_1 + 1, pos_2 - pos_1 - 1));
			}
			for (auto i = aggregateFunctionAttributes.begin(); i != aggregateFunctionAttributes.end(); i++)
			{
				bool exits = false;
				int loc = 0;
				//��������ֶ���
				for (auto attr = tb->GetAttributes().begin(); attr != tb->GetAttributes().end(); attr++, loc++)
				{

					if (attr->get_attr_name() == *i)
					{
						attribute_loc.push_back(loc);
						exits = true;
						break;
					}
				}
				if (exits == false)
				{
					cout << "�ۼ��������õ��ֶ��ڸñ��в����ڣ�" << endl;
					return result;
				}
			}
		}
	}
	if (!isAggregateFunction)
	{
		for (auto i = st.get_select_attribute().begin(); i != st.get_select_attribute().end(); i++)
		{
			bool exits = false;
			int loc = 0;
			//������ѯ���ֶ���
			for (auto attr = tb->GetAttributes().begin(); attr != tb->GetAttributes().end(); attr++, loc++)
			{

				if (*i == "*" || attr->get_attr_name() == *i)
				{
					attribute_loc.push_back(loc);
					exits = true;
					if (*i != "*")
					{
						break;
					}
				}
			}
			if (exits == false)
			{
				cout << "��ѯ���ֶ����ڸñ��в����ڣ�" << endl;
				return result;
			}
		}
	}
	bool has_index = false;
	int index_idx;
	int where_idx;

	//�����index,����index�Ƿ������ڲ�ѯ����������
	if (tb->GetIndexNum() != 0)
	{
		for (auto i = 0; i < tb->GetIndexNum(); i++)
		{
			Index *idx = tb->GetIndex(i);
			for (auto j = 0; j < st.GetWheres().size(); j++)
			{
				if (idx->get_attr_name() == st.GetWheres()[j].key_1 && st.GetWheres()[j].op_type != SIGN_NE)//xj:forB+tree,pre:== SIGN_EQ
				{
					has_index = true;
					index_idx = i;
					where_idx = j;
				}
			}
		}
	}
	//�����ѯ����û��index,���������block
	if (!has_index)
	{
		int block_num = tb->get_first_block_num();
		for (int i = 0; i < tb->get_block_count(); i++)
		{
			BlockInfo *bp = GetBlockInfo(tb, block_num);
			for (int j = 0; j < bp->GetRecordCount(); j++)
			{
				vector<TKey> tuple = GetRecord(tb, block_num, j);
				bool sats = true;
				for (auto k = 0; k < st.GetWheres().size(); k++)
				{
					SQLWhere where = st.GetWheres()[k];
					if (!SatisfyWhere(tb, tuple, where)) sats = false;
				}
				if (sats) tuples.push_back(tuple);
			}
			block_num = bp->GetNextBlockNum();
		}
	}
	//���index�����ڸ��У�����B+����������
	else
	{
		BPlusTree tree(tb->GetIndex(index_idx), buffer_m_, catalog_m_, db_name_);

		//Ϊtkey������
		int type = tb->GetIndex(index_idx)->get_key_type();
		int length = tb->GetIndex(index_idx)->get_key_len();
		string value = st.GetWheres()[where_idx].value;
		TKey dest_key(type, length);
		dest_key.ReadValue(value);

		//xujing:��ֵ��ѯ�뷶Χ��ѯ ��֧
		vector<int> blocknumList = tree.get_value(dest_key, st.GetWheres()[where_idx].op_type, searchType);
		//int blocknum = tree.get_value(dest_key);
		//�õ���ѯ�������
		for (auto bnum = blocknumList.begin(); bnum != blocknumList.end(); bnum++)
		{
			int blocknum = (*bnum);
			if (blocknum != -1)
			{
				int blockoffset = blocknum;
				//ȡ��16λ����ǰ2���ֽڣ�������
				blocknum = blocknum >> 16;
				//ȡ��16λ������2���ֽڣ��������ƫ����
				blocknum = blocknum && 0xffff;
				//�õ����ݿ�źͿ���λ���õ���blockoffset��tuple
				blockoffset = blockoffset & 0xffff;

				vector<TKey> tuple = GetRecord(tb, blocknum, blockoffset);
				bool sats = true;
				for (auto k = 0; k < st.GetWheres().size(); k++)
				{
					SQLWhere where = st.GetWheres()[k];
					if (!SatisfyWhere(tb, tuple, where)) sats = false;
				}
				if (sats) tuples.push_back(tuple);
			}
		}
	}
	if (tuples.size() == 0)
	{
		cout << "�ձ�Empty table��" << endl;
		return result;
	}
	string sline = "";


	//��ӡ������
	for (int i = 0; i < attribute_loc.size(); i++)
	{
		cout << "+----------";
		sline += "+----------";
	}
	cout << "+" << endl;
	sline += "+";
	if (!isAggregateFunction)
	{
		for (int i = 0; i < attribute_loc.size(); i++)
		{
			cout << "| " << setw(9) << left << tb->GetAttributes()[attribute_loc[i]].get_attr_name();
		}
	}
	else
	{
		for (int i = 0; i < st.get_select_attribute().size(); i++)
		{
			cout << "| " << setw(9) << left << st.get_select_attribute()[i];
		}
	}
	cout << "|" << endl;
	cout << sline << endl;

	//xujing:�ۼ�����ʹ��
	int index = 2;//1:��2������
				  //TKey min = Min(tuples, index);//testMin
				  //TKey min = Max(tuples, index);//testMax
				  //TKey* min = Avg(tuples, index);//testAvg:varcharʱ����key_=����
	int min = Count(tuples, index);//testCount
	if (!isAggregateFunction)
	{ //��ӡ���					
		for (auto tuple = tuples.begin(); tuple != tuples.end(); tuple++)
		{
			vector<TKey> reuslt_tuple;
			for (int i = 0; i < attribute_loc.size(); i++)
			{
				//ֻ��ӡѡ����ֶ�
				auto val = tuple->begin() + attribute_loc[i];
				reuslt_tuple.push_back(*val);
				cout << "| " << setw(10) << (*val);
			}

			result.push_back(reuslt_tuple);
			cout << "|" << endl;
			cout << sline << endl;
		}
	}
	else
	{
		/*for (int i = 0; i < attribute_loc.size(); i++)
		{
		vector<TKey> reuslt_tuple;
		for (auto tuple = tuples.begin(); tuple != tuples.end(); tuple++)
		{
		auto val = tuple->begin() + attribute_loc[i];
		reuslt_tuple.push_back(*val);
		}
		result.push_back(reuslt_tuple);
		}*/
		for (int i = 0; i < st.get_select_attribute().size(); i++)
		{
			string s = st.get_select_attribute()[i];
			s.assign(s.substr(0, 3));
			if (s == "cou")
			{
				cout << "| " << setw(9) << left << Count(tuples, attribute_loc[i]);
			}
			else if (s == "min")
			{
				TKey tmp = Min(tuples, attribute_loc[i]);
				cout << "| " << setw(9) << left << tmp;
			}
			else if (s == "max")
			{
				TKey tmp = Max(tuples, attribute_loc[i]);
				cout << "| " << setw(9) << left << tmp;
			}
			else/* (s == "avg")*/
			{
				TKey *tmp = Avg(tuples, attribute_loc[i]);
				cout << "| " << setw(9) << left << (*tmp);
			}
		}
		cout << "|" << endl;
		cout << sline << endl;
	}
	//xujing:�ۼ������������
	if (!isAggregateFunction)
		cout << "| Result | " << setw(10) << min;//(*min);

	cout << "| ��ѯ��ʽ | " << setw(10) << searchType << endl;

	//������ӡ����
	/*if (tb->GetIndexNum() != 0)
	{
	BPlusTree tree(tb->GetIndex(0), buffer_m_, catalog_m_, db_name_);
	tree.print();
	}*/
	return result;
}

//Join��ѯʵ��
void RecordManager::JoinSelect(SQLJoinSelect & st)
{
	int table_count = st.get_table_names().size();
	//�õ�ѡ���������
	vector<string> selected_attributes = st.get_selected_info();

	if (selected_attributes[0] != "*")
	{
		bool isAggregation = false;
		if (selected_attributes[0].find("(") != string::npos)
			isAggregation = true;
		//����SQLStatement����Parse�����Ѿ��жϹ������Ϸ�����������ֻ���ж������Ƿ�Ϸ�
		for (int i = 0; i < selected_attributes.size(); i++)
		{
			string tbName = selected_attributes[i];
			if (isAggregation)
			{
				int pos_1 = tbName.find("(");
				int pos_2 = tbName.find(")");
				tbName.assign(tbName.substr(pos_1 + 1, pos_2 - pos_1 - 1));
			}
			int Pos = tbName.find('.');
			string attrName = tbName.substr(Pos + 1, tbName.length() - Pos - 1);
			tbName.assign(tbName.substr(0, Pos));
			Table *tb_ = catalog_m_->GetDB(db_name_)->GetTable(tbName);
			vector<Attribute> cur_attr = tb_->GetAttributes();
			bool isValidAtrr = false;
			for (int j = 0; j < cur_attr.size(); j++)
			{
				if (cur_attr[j].get_attr_name() == attrName)
				{
					isValidAtrr = true;
					break;
				}
			}
			if (!isValidAtrr)
				throw AttributeNotExistException();
		}
	}

	//�±���ֶ�������
	vector<string> attribute_names;
	//�±���ֶ����ͼ���
	vector<string> attr_types;
	//�±��tuple����
	vector<vector<string>> new_tuples;
	//join֮ǰ��ļ���
	vector<Table> old_tables;
	//�������б��б�����ԣ��Ӷ�����һ�Ŵ��
	for (int i = 0; i < table_count; i++)
	{
		Table *tb = catalog_m_->GetDB(db_name_)->GetTable(st.get_table_names()[i]);
		vector<Attribute> atts = tb->GetAttributes();
		for (auto it = atts.begin(); it != atts.end(); it++)
		{
			attribute_names.push_back(tb->get_tb_name() + "." + it->get_attr_name());
			if (it->get_data_type() == 0)
				attr_types.push_back("int");
			else if (it->get_data_type() == 1)
				attr_types.push_back("float");
			else//char or varchar
			{
				attr_types.push_back("varchar(" + intToString(it->get_length()) + ")");
			}
		}
		old_tables.push_back((*tb));
	}
	string table_name_afer_join = "JOINED_TABLE";
	QueryParser query_parser;

	string create_new_table_sql_ = "create table JOINED_TABLE(";
	int k;
	for (k = 0; k <(attribute_names.size() - 1); k++)
	{
		create_new_table_sql_ += attribute_names[k] + " " + attr_types[k] + ",";
	}
	create_new_table_sql_ += attribute_names[k] + " " + attr_types[k] + ");";
	bool flag = true;
	query_parser.ExecuteSQL("use " + db_name_ + ";", flag);
	//����һ���±�
	query_parser.ExecuteSQL(create_new_table_sql_, flag);

	//catalog_m_->WriteArchiveFile();�˾��в��ɼӣ���Ϊ��һ���Ѿ�������catalog.����д�Ļ��Ḳ�Ǹ��µ����ݣ�
	catalog_m_->ReadArchiveFile();//ֻ��Ѹ��º��Ŀ¼����������

								  //�������������ݵ��±��У�֮����±�ִ��select������Ҫ��������A=����B,to be continued...
	int i = 0;
	if (old_tables.size() == 2)
	{
		/*vector<vector<TKey>> vt1=Select((*seleAll));*/
		//��һ�ű��Tuples
		vector<vector<TKey>> vt1;
		int block_num_1 = old_tables[i].get_first_block_num();
		for (int x = 0; x <old_tables[i].get_block_count(); x++)
		{
			BlockInfo *bp = GetBlockInfo(&old_tables[i], block_num_1);
			for (int j = 0; j < bp->GetRecordCount(); j++)
			{
				vector<TKey> tuple = GetRecord(&old_tables[i], block_num_1, j);
				vt1.push_back(tuple);
			}
			block_num_1 = bp->GetNextBlockNum();
		}

		//�ڶ��ű��Tuples
		vector<vector<TKey>> vt2;
		int block_num_2 = old_tables[i + 1].get_first_block_num();
		for (int x = 0; x <old_tables[i + 1].get_block_count(); x++)
		{
			BlockInfo *bp = GetBlockInfo(&old_tables[i + 1], block_num_2);
			for (int j = 0; j < bp->GetRecordCount(); j++)
			{
				vector<TKey> tuple = GetRecord(&old_tables[i + 1], block_num_2, j);
				vt2.push_back(tuple);
			}
			block_num_2 = bp->GetNextBlockNum();
		}
		int p = vt1.size();
		string tmp;
		for (int j = 0; j < p; j++)
		{//��һ�ű����
			vector<TKey> tmp_key = vt1[j];
			tmp = "";
			for (int k = 0; k < tmp_key.size(); k++)
			{//��һ�ű����
				if (tmp_key[k].get_key_type() == T_INT)
				{
					int a;
					memcpy(&a, tmp_key[k].get_key(), tmp_key[k].get_length());
					tmp += intToString(a) + ",";
				}
				else if (tmp_key[k].get_key_type() == T_FLOAT)
				{
					float a;
					memcpy(&a, tmp_key[k].get_key(), tmp_key[k].get_length());
					string copy = floatToString(a);
					tmp += copy + ",";
				}
				else//T_CHAR
				{
					string copy(tmp_key[k].get_key());
					copy = "'" + copy + "'";
					tmp += copy + ",";
				}
			}
			string tmp_1 = tmp;
			int q = vt2.size();
			for (int y = 0; y < q; y++)
			{//�ڶ��ű����
				tmp_1 = tmp;
				vector<TKey> tmp_key_1 = vt2[y];
				for (int k = 0; k < tmp_key_1.size(); k++)
				{//�ڶ��ű����
					if (tmp_key_1[k].get_key_type() == T_INT)
					{
						int a;
						memcpy(&a, tmp_key_1[k].get_key(), tmp_key_1[k].get_length());
						if (k == (tmp_key_1.size() - 1))
							tmp_1 += intToString(a);
						else
							tmp_1 += intToString(a) + ",";
					}
					else if (tmp_key_1[k].get_key_type() == T_FLOAT)
					{
						float a;
						memcpy(&a, tmp_key_1[k].get_key(), tmp_key_1[k].get_length());
						string copy = floatToString(a);
						if (k == (tmp_key_1.size() - 1))
							tmp_1 += copy;
						else
							tmp_1 += copy + ",";
					}
					else//T_CHAR
					{
						string copy(tmp_key_1[k].get_key());
						copy = "'" + copy + "'";
						if (k == (tmp_key_1.size() - 1))
							tmp_1 += copy;
						else
							tmp_1 += copy + ",";
					}
				}
				string insertStatement = "insert into JOINED_TABLE values(" + tmp_1 + ")";
				bool output_info_flag = false;//���������ɹ���Ϣ
				query_parser.ExecuteSQL(insertStatement, output_info_flag);
				catalog_m_->ReadArchiveFile();
			}
		}
	}
	//����������select ���ԣ����ؽ���󣬽�JOIEND_TABLEɾ������
	string select_from_joined_table = "select ";
	string selectAttr = "";
	for (int i = 0; i < selected_attributes.size(); i++)
		selectAttr += selected_attributes[i] + ",";
	//�Ƴ����һ������
	selectAttr.assign(selectAttr.substr(0, selectAttr.length() - 1));
	vector<SQLWhere> conditions = st.get_wheres();
	if (conditions.size() == 0)
	{
		select_from_joined_table += selectAttr + " from JOINED_TABLE;";
	}
	else
	{
		select_from_joined_table += selectAttr + " from JOINED_TABLE where ";

		for (int i = 0; i < conditions.size(); i++)
		{
			string op;
			switch (conditions[i].op_type)
			{
			case SIGN_EQ:op = "="; break;
			case SIGN_GE:op = ">="; break;
			case SIGN_GT:op = ">"; break;
			case SIGN_LE:op = "<="; break;
			case SIGN_LT:op = "<"; break;
			case SIGN_NE:op = "!="; break;
			default:break;
			}
			if (op == "")
				throw SyntaxErrorException();
			string tmp = "";
			if (conditions[i].data_type == T_CHAR)
				tmp = conditions[i].key_1 + op + "'" + conditions[i].value + "'";
			else tmp = conditions[i].key_1 + op + conditions[i].value + conditions[i].key_2;
			select_from_joined_table += tmp + " and ";
		}
		select_from_joined_table.assign(select_from_joined_table.substr(0, select_from_joined_table.length() - 5) + ";");
	}
	query_parser.ExecuteSQL(select_from_joined_table, flag);
	query_parser.ExecuteSQL("drop table JOINED_TABLE;", flag);
	catalog_m_->ReadArchiveFile();
}

void RecordManager::Delete(SQLDelete& st)
{
	Table *tb = catalog_m_->GetDB(db_name_)->GetTable(st.get_tb_name());
	bool has_index = false;
	int index_idx = -2;
	int where_idx;

	//���Ƿ���index
	if (tb->GetIndexNum() != 0)
	{
		index_idx = -1;
		for (auto i = 0; i < tb->GetIndexNum(); i++)
		{
			Index *idx = tb->GetIndex(i);
			for (auto j = 0; j < st.GetWheres().size(); j++)
			{
				if (idx->get_attr_name() == st.GetWheres()[j].key_1 && st.GetWheres()[j].op_type == SIGN_EQ)
				{
					has_index = true;
					index_idx = i;
					where_idx = j;
				}
			}
		}
	}
	//���index����������ɾ���У��������
	if (!has_index)
	{
		int block_num = tb->get_first_block_num();
		for (int i = 0; i < tb->get_block_count(); i++)
		{
			BlockInfo *bp = GetBlockInfo(tb, block_num);
			int count_ = bp->GetRecordCount();
			for (int j = 0; j < count_; j++)
			{
				vector<TKey> tuple = GetRecord(tb, block_num, j);
				bool sats = true;
				for (int k = 0; k < st.GetWheres().size(); k++)
				{
					SQLWhere where = st.GetWheres()[k];
					if (!SatisfyWhere(tb, tuple, where)) sats = false;
				}
				if (sats)
				{
					//ɾ���ü�¼
					DeleteRecord(tb, block_num, j);
					if (tb->GetIndexNum() != 0)
					{
						for (index_idx = 0; index_idx < tb->GetIndexNum(); index_idx++)
						{
							BPlusTree tree(tb->GetIndex(index_idx), buffer_m_, catalog_m_, db_name_);
							int idx = -1;
							for (int i = 0; i < tb->GetAttributeNum(); i++)
							{
								if (tb->GetAttributes()[i].get_attr_name() == tb->GetIndex(index_idx)->get_attr_name())
									idx = i;
							}
							tree.remove(tuple[idx]);
						}

					}
				}
			}
			block_num = bp->GetNextBlockNum();
		}
	}
	//���index��������ɾ����
	else
	{
		BPlusTree tree(tb->GetIndex(index_idx), buffer_m_, catalog_m_, db_name_);

		//Ϊsearch����tkey
		int type = tb->GetIndex(index_idx)->get_key_type();
		int length = tb->GetIndex(index_idx)->get_key_len();
		string value = st.GetWheres()[where_idx].value;
		TKey dest_key(type, length);
		dest_key.ReadValue(value);

		int blocknum = tree.get_value(dest_key);

		if (blocknum != -1)
		{
			int blockoffset = blocknum;
			blocknum = blocknum >> 16;
			blocknum = blocknum && 0xffff;
			blockoffset = blockoffset & 0xffff;

			vector<TKey> tuple = GetRecord(tb, blocknum, blockoffset);
			bool sats = true;
			for (int k = 0; k < st.GetWheres().size(); k++)
			{
				SQLWhere where = st.GetWheres()[k];
				if (!SatisfyWhere(tb, tuple, where)) sats = false;
			}
			if (sats)
			{
				//���ü�¼ɾ��
				DeleteRecord(tb, blocknum, blockoffset);
				tree.remove(dest_key);
			}
		}
	}
	buffer_m_->WriteToDisk();
	cout << "ɾ���ɹ���" << endl;
}

void RecordManager::Update(SQLUpdate& st)
{
	Table *tb = catalog_m_->GetDB(db_name_)->GetTable(st.get_tb_name());

	vector<int> indices;
	vector<TKey> tuple;
	int primary_key_index = -1;
	int affect_index = -1;

	//������
	for (int i = 0; i < tb->GetAttributes().size(); ++i)
	{
		if (tb->GetAttributes()[i].get_attr_type() == 1)
			primary_key_index = i;
	}

	for (int i = 0; i < st.GetKeyValues().size(); i++)
	{
		int index = tb->GetAttributeIndex(st.GetKeyValues()[i].key);
		indices.push_back(index);
		TKey value(tb->GetAttributes()[index].get_data_type(), tb->GetAttributes()[index].get_length());
		value.ReadValue(st.GetKeyValues()[i].value);
		tuple.push_back(value);

		if (index == primary_key_index) affect_index = i;
	}
	if (affect_index != -1)
	{
		if (tb->GetIndexNum() != 0)
		{
			BPlusTree tree(tb->GetIndex(0), buffer_m_, catalog_m_, db_name_);
			if (tree.get_value(tuple[affect_index]) != -1)
				throw PrimaryKeyConflictException();
		}
		else
		{
			int block_num = tb->get_first_block_num();
			for (int i = 0; i < tb->get_block_count(); i++)
			{
				BlockInfo *bp = GetBlockInfo(tb, block_num);

				for (int j = 0; j < bp->GetRecordCount(); j++)
				{
					vector<TKey> tp = GetRecord(tb, block_num, j);
					if (tp[primary_key_index] == tuple[affect_index])
						throw PrimaryKeyConflictException();
				}
				block_num = bp->GetNextBlockNum();
			}
		}
	}
	int block_num = tb->get_first_block_num();
	for (int i = 0; i < tb->get_block_count(); i++)
	{
		BlockInfo *bp = GetBlockInfo(tb, block_num);

		for (int j = 0; j < bp->GetRecordCount(); j++)
		{
			vector<TKey> tp = GetRecord(tb, block_num, j);
			bool sats = true;
			for (int k = 0; k < st.GetWheres().size(); k++)
			{
				SQLWhere where = st.GetWheres()[k];
				if (!SatisfyWhere(tb, tp, where)) sats = false;
			}
			if (sats)
			{
				/* remove from index. */
				if (tb->GetIndexNum() != 0)
				{
					BPlusTree tree(tb->GetIndex(0), buffer_m_, catalog_m_, db_name_);
					int idx = -1;
					for (int i = 0; i < tb->GetAttributeNum(); i++)
					{
						if (tb->GetAttributes()[i].get_attr_name() == tb->GetIndex(0)->get_attr_name())
							idx = i;
					}
					tree.remove(tuple[idx]);
				}
				UpdateRecord(tb, block_num, j, indices, tuple);

				tp = GetRecord(tb, block_num, j);
				/* add index for new key. */
				if (tb->GetIndexNum() != 0)
				{
					BPlusTree tree(tb->GetIndex(0), buffer_m_, catalog_m_, db_name_);
					int idx = -1;
					for (int i = 0; i < tb->GetAttributeNum(); i++)
					{
						if (tb->GetAttributes()[i].get_attr_name() == tb->GetIndex(0)->get_attr_name())
							idx = i;
					}
					tree.add(tp[idx], block_num, j);
				}
			}
		}
		block_num = bp->GetNextBlockNum();
	}
	buffer_m_->WriteToDisk();
	cout << "���³ɹ���" << endl;
}
//���ݱ�Ŀ���õ�����Ϣ
BlockInfo* RecordManager::GetBlockInfo(Table* tbl, int block_num)
{
	if (block_num == -1) return NULL;
	BlockInfo* block = buffer_m_->GetFileBlock(db_name_, tbl->get_tb_name(), 0, block_num);
	return block;
}
//����tb1�ĵ�block_num����ĵ�offset��tuple
vector<TKey> RecordManager::GetRecord(Table* tbl, int block_num, int offset)
{
	vector<TKey> keys;
	BlockInfo *bp = GetBlockInfo(tbl, block_num);
	char *content = bp->get_data() + 12 + offset * tbl->get_record_length();

	for (int i = 0; i < tbl->GetAttributeNum(); ++i)
	{
		int value_type = tbl->GetAttributes()[i].get_data_type();
		int length = tbl->GetAttributes()[i].get_length();
		//һ������ֵ���������ͺ����Գ���
		TKey tmp(value_type, length);
		//��contentָ����length���ֽڸ��Ƹ�tmp��key
		memcpy(tmp.get_key(), content, length);
		//cout << "RecordManager::GetRecord::memcpy :" << content << " to " << tmp.get_key() << endl;
		keys.push_back(tmp);
		content += length;
	}
	return keys;
}
//ɾ��tb1��block_num��ĵ�offset��tuple
void RecordManager::DeleteRecord(Table* tbl, int block_num, int offset)
{
	BlockInfo *bp = GetBlockInfo(tbl, block_num);
	char *content = bp->get_data() + offset * tbl->get_record_length() + 12;
	char *replace = bp->get_data() + (bp->GetRecordCount() - 1) * (tbl->get_record_length()) + 12;
	//�Ѵ�ɾ��¼���Ƶ��ÿ��β��
	memcpy(content, replace, tbl->get_record_length());
	//��¼������һ
	bp->DecreaseRecordCount();
	//���ɾ���󣬸ÿ��¼Ϊ0��������ӵ��������������
	if (bp->GetRecordCount() == 0)
	{
		int prevnum = bp->GetPrevBlockNum();
		int nextnum = bp->GetNextBlockNum();
		if (prevnum != -1)
		{//��ǰһ���next��Ϊ��ǰ���next
			BlockInfo *pbp = GetBlockInfo(tbl, prevnum);
			pbp->SetNextBlockNum(nextnum);
			buffer_m_->WriteBlock(pbp);
		}
		if (nextnum != -1)
		{//����һ���previous��Ϊ��ǰ���previous
			BlockInfo *nbp = GetBlockInfo(tbl, nextnum);
			nbp->SetPrevBlockNum(prevnum);
			//��bp��Ϊdirty�����޸Ĺ�
			buffer_m_->WriteBlock(nbp);
		}
		//�õ����п��ÿ�ĵ�һ��ָ��
		BlockInfo *firstrubbish = GetBlockInfo(tbl, tbl->get_first_block_num());

		bp->SetNextBlockNum(-1);
		bp->SetPrevBlockNum(-1);
		if (firstrubbish != NULL)
		{
			//�����������ŵ�firstrubbish֮ǰ��Ҳ���ǰ�����ճ����Ŀ���ڿ��ÿ�Ķ��׵�ǰ��
			firstrubbish->SetPrevBlockNum(block_num);
			bp->SetNextBlockNum(firstrubbish->get_block_num());
		}
		//������µ���������Ϊ���������
		tbl->set_first_rubbish_num(block_num);
	}
	//��bp��Ϊdirty�����޸Ĺ�
	buffer_m_->WriteBlock(bp);
}

void RecordManager::UpdateRecord(Table* tbl, int block_num, int offset, vector<int>& indices, vector<TKey>& values)
{
	BlockInfo *bp = GetBlockInfo(tbl, block_num);
	char *content = bp->get_data() + offset * tbl->get_record_length() + 12;

	for (int i = 0; i < tbl->GetAttributeNum(); i++)
	{
		auto iter = find(indices.begin(), indices.end(), i);
		if (iter != indices.end())
			memcpy(content, values[iter - indices.begin()].get_key(), values[iter - indices.begin()].get_length());
		content += tbl->GetAttributes()[i].get_length();
	}
	buffer_m_->WriteBlock(bp);
}


bool RecordManager::SatisfyWhere(Table* tbl, vector<TKey> keys, SQLWhere where)
{
	int idx = -1;
	string key_2 = where.key_2;
	//����1 op ����2
	if (key_2 != "")
	{
		int idx_1 = -1;
		int idx_2 = -1;
		for (int i = 0; i < tbl->GetAttributeNum(); ++i)
		{
			if (tbl->GetAttributes()[i].get_attr_name() == where.key_1)
			{
				idx_1 = i;
			}
			if (tbl->GetAttributes()[i].get_attr_name() == where.key_2)
			{
				idx_2 = i;
			}
			if (idx_1 != -1 && idx_2 != -1)
			{
				break;
			}
		}
		switch (where.op_type)
		{
		case SIGN_EQ:
			return keys[idx_1] == keys[idx_2];
			break;
		case SIGN_NE:
			return keys[idx_1] != keys[idx_2];
			break;
		case SIGN_LT:
			return keys[idx_1] < keys[idx_2];
			break;
		case SIGN_GT:
			return keys[idx_1] > keys[idx_2];
			break;
		case SIGN_LE:
			return keys[idx_1] <= keys[idx_2];
			break;
		case SIGN_GE:
			return keys[idx_1] >= keys[idx_2];
			break;
		default:
			return false;
			break;
		}
	}
	else
	{
		for (int i = 0; i < tbl->GetAttributeNum(); ++i)
		{
			if (tbl->GetAttributes()[i].get_attr_name() == where.key_1)
			{
				idx = i;
				break;
			}
		}
		TKey tmp(tbl->GetAttributes()[idx].get_data_type(), tbl->GetAttributes()[idx].get_length());
		tmp.ReadValue(where.value.c_str());

		switch (where.op_type)
		{
		case SIGN_EQ:
			return keys[idx] == tmp;
			break;
		case SIGN_NE:
			return keys[idx] != tmp;
			break;
		case SIGN_LT:
			return keys[idx] < tmp;
			break;
		case SIGN_GT:
			return keys[idx] > tmp;
			break;
		case SIGN_LE:
			return keys[idx] <= tmp;
			break;
		case SIGN_GE:
			return keys[idx] >= tmp;
			break;
		default:
			return false;
			break;
		}
	}
}


/**********************                  �ۼ�����ʵ��                      ********************************/
/*����tuple�в�û�б�ͷ��Ϣ������sql�����ۼ�����ʱ��������ۼ������Ǳ�ĵڼ��У�index*/
TKey RecordManager::Min(vector<vector<TKey> > tuples, int MinIndex)
{
	TKey * temp = nullptr;//����ֵ
	int j = 0;
	for (auto tuple = tuples.begin(); tuple != tuples.end(); tuple++, j++)
	{
		int i = 0;
		for (auto val = tuple->begin(); val != tuple->end(); val++, i++)
		{
			if (i == MinIndex)
			{
				if (j == 0)
				{
					temp = new TKey(*(val));
				}
				if ((*temp) > (*val))
					(*temp) = (*val);
			}
		}
	}
	return (*temp);
}
TKey RecordManager::Max(vector<vector<TKey>> tuples, int MaxIndex)
{
	TKey * temp = nullptr;//����ֵ
	int j = 0;
	for (auto tuple = tuples.begin(); tuple != tuples.end(); tuple++, j++)
	{
		int i = 0;
		for (auto val = tuple->begin(); val != tuple->end(); val++, i++)
		{
			if (i == MaxIndex)
			{
				if (j == 0)
				{
					temp = new TKey(*(val));
				}
				if ((*temp) < (*val))
					(*temp) = (*val);
			}
		}
	}
	return (*temp);
}

//����û��Group By ���Զ����Ե�countֱ�ӵȼ���tuple��count��Index��ʱû��
int RecordManager::Count(vector<vector<TKey> > tuples, int Index)
{
	return tuples.size();
}

TKey* RecordManager::Avg(vector<vector<TKey>> tuples, int MinIndex)
{
	TKey * temp = nullptr;//����ֵ
	int j = 0;
	for (auto tuple = tuples.begin(); tuple != tuples.end(); tuple++, j++)
	{
		int i = 0;
		for (auto val = tuple->begin(); val != tuple->end(); val++, i++)
		{
			if (i == MinIndex)
			{
				if (j == 0)
				{
					temp = new TKey(*(val));
				}
				else
					(*temp) += (*val);
			}
		}
	}
	if (temp != nullptr)
		(*temp) /= j;
	return (temp);
}