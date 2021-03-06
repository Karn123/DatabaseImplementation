//Implemented by Lai ZhengMin & Xu Jing

//定长记录=sum(属性),块的编号是从大到小的(block->preNum=block->getNum()+1)
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
	//这个待插记录有多少个key value
	int values_size = st.GetValues().size();
	//根据表名，从目录文件下拿到数据库中的表
	Table *tb = catalog_m_->GetDB(db_name_)->GetTable(tb_name);
	if (tb == NULL) throw TableNotExistException();

	//一块（4K，头12 bytes）能装多少个记录（tuple）
	int max_count = (4096 - 12) / (tb->get_record_length());

	vector<TKey> tkey_values;

	int primary_key_index = -1;

	//遍历待插记录，看是否有主键，并把数据类型和键值存到tkey_values中
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
	//如果有主键
	if (primary_key_index != -1)
	{
		//如果有索引
		if (tb->GetIndexNum() != 0)
		{
			BPlusTree tree(tb->GetIndex(0), buffer_m_, catalog_m_, db_name_);
			if (tree.get_value(tkey_values[primary_key_index]) != -1)
				throw PrimaryKeyConflictException();
		}
		//无索引
		else
		{
			//拿到该表的起始块号 
			int block_num = tb->get_first_block_num();
			//遍历表中的块，看插入的数据是否会发生主键冲突
			for (int i = 0; i < tb->get_block_count(); i++)
			{
				//拿到该块号对应的块信息
				BlockInfo *bp = GetBlockInfo(tb, block_num);
				for (int j = 0; j < bp->GetRecordCount(); j++)
				{
					//拿到块内的第j个记录
					vector<TKey> tuple = GetRecord(tb, block_num, j);
					//如果主键属性值发生冲突
					if (tuple[primary_key_index] == tkey_values[primary_key_index])
						throw PrimaryKeyConflictException();
				}
				block_num = bp->GetNextBlockNum();
			}
		}
	}
	char *content;
	//可用块的起始块号
	int use_block = tb->get_first_block_num();
	//垃圾块的起始块号
	int first_rubbish_block = tb->get_first_rubbish_num();
	//记录上一次使用的块
	int last_use_block;
	int blocknum, offset;
	//无可用块
	while (use_block != -1)
	{
		last_use_block = use_block;
		BlockInfo *bp = GetBlockInfo(tb, use_block);
		//块满了
		if (bp->GetRecordCount() == max_count)
		{
			use_block = bp->GetNextBlockNum();
			continue;
		}
		//未满，则往record尾部插入记录
		content = bp->GetContentAdress() + bp->GetRecordCount() * tb->get_record_length();
		//复制一个tuple，也就是从块的空闲位置插入一个tuple
		for (auto iter = tkey_values.begin(); iter != tkey_values.end(); iter++)
		{
			//复制给content
			memcpy(content, iter->get_key(), iter->get_length());
			content += iter->get_length();
		}
		bp->SetRecordCount(1 + bp->GetRecordCount());
		//记录插入的块号
		blocknum = use_block;
		//记录块内偏移量
		offset = bp->GetRecordCount() - 1;
		//更新块信息
		buffer_m_->WriteBlock(bp);

		//如果有索引
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
		//将更新后的结果写回磁盘
		buffer_m_->WriteToDisk();
		//将目录信息写回磁盘
		catalog_m_->WriteArchiveFile();
		if (flag)
			cout << "插入成功！" << endl;
		return;
	}
	//如果无可用块但有垃圾块（已经被回收的空块），则插到垃圾块中
	if (first_rubbish_block != -1)
	{
		//拿到第一个垃圾块
		BlockInfo *bp = GetBlockInfo(tb, first_rubbish_block);
		content = bp->GetContentAdress();
		//复制record到content中
		for (auto iter = tkey_values.begin(); iter != tkey_values.end(); iter++)
		{
			memcpy(content, iter->get_key(), iter->get_length());
			content += iter->get_length();
		}
		//垃圾块的记录为1
		bp->SetRecordCount(1);
		//得到跳出while循环之前的block，他的nextBlockNum为-1
		BlockInfo *last_use_block_p = GetBlockInfo(tb, last_use_block);
		//把first_rubbish_block挂在他的后面
		last_use_block_p->SetNextBlockNum(first_rubbish_block);
		//把垃圾块的指针后移一位
		tb->set_first_rubbish_num(bp->GetNextBlockNum());
		//该块的前一块为last_use_block
		bp->SetPrevBlockNum(last_use_block);

		//表明该块的后面无可用块
		bp->SetNextBlockNum(-1);
		//更新插入块的块号为first_rubbish_block
		blocknum = first_rubbish_block;
		//快内偏移量为0，因为它是该块的第一条记录
		offset = 0;
		//将bp和last_use_block_p设为脏块，等待最后写回磁盘
		buffer_m_->WriteBlock(bp);
		buffer_m_->WriteBlock(last_use_block_p);
	}
	else//如果当前既无可用块也无垃圾块供插入，则要创建一个新块
	{
		int next_block = tb->get_first_block_num();
		//如果不是第一次插入
		if (next_block != -1)
		{
			BlockInfo *up = GetBlockInfo(tb, tb->get_first_block_num());
			//设置它之前的块的编号为block_count（比自己大1）
			up->SetPrevBlockNum(tb->get_block_count());
			buffer_m_->WriteBlock(up);
		}
		//设置第一个可用块的编号
		tb->set_first_block_num(tb->get_block_count());
		//创建一个新块
		BlockInfo *bp = GetBlockInfo(tb, tb->get_first_block_num());
		//它前面无块
		bp->SetPrevBlockNum(-1);
		//将next_block挂在他后面，prev_num要比自己的num大
		bp->SetNextBlockNum(next_block);
		//将记录加1，把待插记录存到bp块中
		bp->SetRecordCount(1);
		content = bp->GetContentAdress();
		for (auto iter = tkey_values.begin(); iter != tkey_values.end(); iter++)
		{
			memcpy(content, iter->get_key(), iter->get_length());
			content += iter->get_length();
		}
		//更新插入的块号
		blocknum = tb->get_block_count();
		//块内偏移量为0
		offset = 0;
		//设为脏块
		buffer_m_->WriteBlock(bp);
		//将表的块数加1
		tb->IncreaseBlockCount();
	}
	//如果有index,把记录插到index中
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
	//将buffer写回磁盘
	buffer_m_->WriteToDisk();
	catalog_m_->WriteArchiveFile();
	//代表是否要输出插入成功信息
	if (flag)
		cout << "插入成功！" << endl;
}
//Select 函数 ，支持数据类型相同的属性A=属性B 查询。
vector<vector<TKey>> RecordManager::Select(SQLSelect& st)
{
	string searchType = "普通查询";
	Table *tb = catalog_m_->GetDB(db_name_)->GetTable(st.get_tb_name());

	//筛选的字段的下标集合
	vector<int> attribute_loc;
	vector<vector<TKey> > tuples;
	vector<vector<TKey>>result;
	bool isAggregateFunction = false;
	//聚集函数作用的属性
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
				//遍历表的字段名
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
					cout << "聚集函数作用的字段在该表中不存在！" << endl;
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
			//遍历查询的字段名
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
				cout << "查询的字段名在该表中不存在！" << endl;
				return result;
			}
		}
	}
	bool has_index = false;
	int index_idx;
	int where_idx;

	//如果有index,看看index是否作用于查询的属性列上
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
	//如果查询的列没有index,则遍历所有block
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
	//如果index作用于该列，则用B+树进行搜索
	else
	{
		BPlusTree tree(tb->GetIndex(index_idx), buffer_m_, catalog_m_, db_name_);

		//为tkey建索引
		int type = tb->GetIndex(index_idx)->get_key_type();
		int length = tb->GetIndex(index_idx)->get_key_len();
		string value = st.GetWheres()[where_idx].value;
		TKey dest_key(type, length);
		dest_key.ReadValue(value);

		//xujing:单值查询与范围查询 分支
		vector<int> blocknumList = tree.get_value(dest_key, st.GetWheres()[where_idx].op_type, searchType);
		//int blocknum = tree.get_value(dest_key);
		//得到查询结果集合
		for (auto bnum = blocknumList.begin(); bnum != blocknumList.end(); bnum++)
		{
			int blocknum = (*bnum);
			if (blocknum != -1)
			{
				int blockoffset = blocknum;
				//取高16位，即前2个字节，代表块号
				blocknum = blocknum >> 16;
				//取低16位，即后2个字节，代表块内偏移量
				blocknum = blocknum && 0xffff;
				//拿到根据块号和块内位移拿到第blockoffset个tuple
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
		cout << "空表（Empty table）" << endl;
		return result;
	}
	string sline = "";


	//打印属性名
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

	//xujing:聚集函数使用
	int index = 2;//1:第2列属性
				  //TKey min = Min(tuples, index);//testMin
				  //TKey min = Max(tuples, index);//testMax
				  //TKey* min = Avg(tuples, index);//testAvg:varchar时返回key_=“”
	int min = Count(tuples, index);//testCount
	if (!isAggregateFunction)
	{ //打印结果					
		for (auto tuple = tuples.begin(); tuple != tuples.end(); tuple++)
		{
			vector<TKey> reuslt_tuple;
			for (int i = 0; i < attribute_loc.size(); i++)
			{
				//只打印选择的字段
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
	//xujing:聚集函数测试输出
	if (!isAggregateFunction)
		cout << "| Result | " << setw(10) << min;//(*min);

	cout << "| 查询方式 | " << setw(10) << searchType << endl;

	//索引打印测试
	/*if (tb->GetIndexNum() != 0)
	{
	BPlusTree tree(tb->GetIndex(0), buffer_m_, catalog_m_, db_name_);
	tree.print();
	}*/
	return result;
}

//Join查询实现
void RecordManager::JoinSelect(SQLJoinSelect & st)
{
	int table_count = st.get_table_names().size();
	//拿到选择的属性名
	vector<string> selected_attributes = st.get_selected_info();

	if (selected_attributes[0] != "*")
	{
		bool isAggregation = false;
		if (selected_attributes[0].find("(") != string::npos)
			isAggregation = true;
		//由于SQLStatement类中Parse函数已经判断过表名合法，所有这里只需判断属性是否合法
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

	//新表的字段名集合
	vector<string> attribute_names;
	//新表的字段类型集合
	vector<string> attr_types;
	//新表的tuple集合
	vector<vector<string>> new_tuples;
	//join之前表的集合
	vector<Table> old_tables;
	//遍历所有表中表的属性，从而创建一张大表
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
	//创建一张新表
	query_parser.ExecuteSQL(create_new_table_sql_, flag);

	//catalog_m_->WriteArchiveFile();此句切不可加！因为上一句已经更新了catalog.现在写的话会覆盖更新的内容！
	catalog_m_->ReadArchiveFile();//只需把更新后的目录读出来即可

								  //接下来插入数据到新表中，之后对新表执行select，所以要做到属性A=属性B,to be continued...
	int i = 0;
	if (old_tables.size() == 2)
	{
		/*vector<vector<TKey>> vt1=Select((*seleAll));*/
		//第一张表的Tuples
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

		//第二张表的Tuples
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
		{//第一张表的行
			vector<TKey> tmp_key = vt1[j];
			tmp = "";
			for (int k = 0; k < tmp_key.size(); k++)
			{//第一张表的列
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
			{//第二张表的行
				tmp_1 = tmp;
				vector<TKey> tmp_key_1 = vt2[y];
				for (int k = 0; k < tmp_key_1.size(); k++)
				{//第二张表的列
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
				bool output_info_flag = false;//不输出插入成功消息
				query_parser.ExecuteSQL(insertStatement, output_info_flag);
				catalog_m_->ReadArchiveFile();
			}
		}
	}
	//接下来就是select 属性，返回结果后，将JOIEND_TABLE删除即可
	string select_from_joined_table = "select ";
	string selectAttr = "";
	for (int i = 0; i < selected_attributes.size(); i++)
		selectAttr += selected_attributes[i] + ",";
	//移除最后一个逗号
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

	//看是否有index
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
	//如果index不是作用于删除列，则遍历块
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
					//删除该记录
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
	//如果index是作用于删除列
	else
	{
		BPlusTree tree(tb->GetIndex(index_idx), buffer_m_, catalog_m_, db_name_);

		//为search创建tkey
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
				//将该记录删除
				DeleteRecord(tb, blocknum, blockoffset);
				tree.remove(dest_key);
			}
		}
	}
	buffer_m_->WriteToDisk();
	cout << "删除成功！" << endl;
}

void RecordManager::Update(SQLUpdate& st)
{
	Table *tb = catalog_m_->GetDB(db_name_)->GetTable(st.get_tb_name());

	vector<int> indices;
	vector<TKey> tuple;
	int primary_key_index = -1;
	int affect_index = -1;

	//找主键
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
	cout << "更新成功！" << endl;
}
//根据表的块号拿到块信息
BlockInfo* RecordManager::GetBlockInfo(Table* tbl, int block_num)
{
	if (block_num == -1) return NULL;
	BlockInfo* block = buffer_m_->GetFileBlock(db_name_, tbl->get_tb_name(), 0, block_num);
	return block;
}
//返回tb1的第block_num块里的第offset个tuple
vector<TKey> RecordManager::GetRecord(Table* tbl, int block_num, int offset)
{
	vector<TKey> keys;
	BlockInfo *bp = GetBlockInfo(tbl, block_num);
	char *content = bp->get_data() + 12 + offset * tbl->get_record_length();

	for (int i = 0; i < tbl->GetAttributeNum(); ++i)
	{
		int value_type = tbl->GetAttributes()[i].get_data_type();
		int length = tbl->GetAttributes()[i].get_length();
		//一个属性值，数据类型和属性长度
		TKey tmp(value_type, length);
		//将content指针后的length个字节复制给tmp的key
		memcpy(tmp.get_key(), content, length);
		//cout << "RecordManager::GetRecord::memcpy :" << content << " to " << tmp.get_key() << endl;
		keys.push_back(tmp);
		content += length;
	}
	return keys;
}
//删除tb1的block_num块的第offset个tuple
void RecordManager::DeleteRecord(Table* tbl, int block_num, int offset)
{
	BlockInfo *bp = GetBlockInfo(tbl, block_num);
	char *content = bp->get_data() + offset * tbl->get_record_length() + 12;
	char *replace = bp->get_data() + (bp->GetRecordCount() - 1) * (tbl->get_record_length()) + 12;
	//把待删记录复制到该块的尾部
	memcpy(content, replace, tbl->get_record_length());
	//记录数量减一
	bp->DecreaseRecordCount();
	//如果删除后，该块记录为0，则把它加到垃圾块的链表里
	if (bp->GetRecordCount() == 0)
	{
		int prevnum = bp->GetPrevBlockNum();
		int nextnum = bp->GetNextBlockNum();
		if (prevnum != -1)
		{//则将前一块的next置为当前块的next
			BlockInfo *pbp = GetBlockInfo(tbl, prevnum);
			pbp->SetNextBlockNum(nextnum);
			buffer_m_->WriteBlock(pbp);
		}
		if (nextnum != -1)
		{//则将下一块的previous置为当前块的previous
			BlockInfo *nbp = GetBlockInfo(tbl, nextnum);
			nbp->SetPrevBlockNum(prevnum);
			//将bp置为dirty，被修改过
			buffer_m_->WriteBlock(nbp);
		}
		//拿到表中可用块的第一个指针
		BlockInfo *firstrubbish = GetBlockInfo(tbl, tbl->get_first_block_num());

		bp->SetNextBlockNum(-1);
		bp->SetPrevBlockNum(-1);
		if (firstrubbish != NULL)
		{
			//将这个垃圾块放到firstrubbish之前，也就是把这个空出来的块挂在可用块的队首的前面
			firstrubbish->SetPrevBlockNum(block_num);
			bp->SetNextBlockNum(firstrubbish->get_block_num());
		}
		//把这个新的垃圾块置为垃圾块队首
		tbl->set_first_rubbish_num(block_num);
	}
	//将bp置为dirty，被修改过
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
	//属性1 op 属性2
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


/**********************                  聚集函数实现                      ********************************/
/*由于tuple中并没有表头信息，所以sql解析聚集运算时，需解析聚集对象是表的第几列：index*/
TKey RecordManager::Min(vector<vector<TKey> > tuples, int MinIndex)
{
	TKey * temp = nullptr;//返回值
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
	TKey * temp = nullptr;//返回值
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

//由于没有Group By 所以对属性的count直接等价于tuple的count。Index暂时没用
int RecordManager::Count(vector<vector<TKey> > tuples, int Index)
{
	return tuples.size();
}

TKey* RecordManager::Avg(vector<vector<TKey>> tuples, int MinIndex)
{
	TKey * temp = nullptr;//返回值
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