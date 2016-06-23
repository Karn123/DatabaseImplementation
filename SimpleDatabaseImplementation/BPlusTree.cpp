//��飺B+������
//Implemented by Xu Jing
//���ã�ʵ�����B+����
#include <iomanip>
#include<iostream>
#include "BPlusTree.h"
#include "Exceptions.h"
#include "ConstValue.h"
using namespace std;

BPlusTree::BPlusTree(Index* idx, BufferManager* bm, CatalogManager* cm, string dbname)
{
	buffer_m_ = bm;
	catalog_m_ = cm;
	idx_ = idx;
	degree_ = 2 * idx_->get_rank() + 1;
	db_name_ = dbname;
}

BPlusTree::~BPlusTree(void) {}

void BPlusTree::InitTree()
{
	BTNode *root_node = new BTNode(this, true, get_new_blocknum(), true);
	idx_->set_root(0);
	idx_->set_leaf_head(idx_->get_root());
	idx_->set_key_count(0);
	idx_->set_node_count(1);
	idx_->set_level(1);
	root_node->set_next_leaf(-1);
}

Index* BPlusTree::GetIndex() { return idx_; }
BufferManager* BPlusTree::GetBufferManager() { return buffer_m_; }
CatalogManager* BPlusTree::GetCatalogManager() { return catalog_m_; }
int BPlusTree::get_degree() { return degree_; }
string BPlusTree::get_db_name() { return db_name_; }
/*�ڵ�block_num�����ϼ�Ԫ��key ƫ��offset*/
bool BPlusTree::add(TKey& key, int block_num, int offset)
{
	int value = (block_num << 16) | offset;/*��offset�����ݼ�¼��offset��һ��16λ�����Ƴ��ȵ�����:0000000��000|offset->��ȡoffset�ĺ�16λ����*/
	if (idx_->get_root() == -1)
		InitTree();

	FindNodeParam fnp = search(idx_->get_root(), key);
	if (!fnp.flag)
	{
		fnp.pnode->add(key, value);
		idx_->IncreaseKeyCount();
		if (fnp.pnode->get_count() == degree_)
			return spiltForAdd(fnp.pnode->get_block_num());
		return true;
	}
	return false;
}
/*��Ԫ�غ����B+��:����*/
bool BPlusTree::spiltForAdd(int node)
{
	BTNode *pnode = get_node(node);
	TKey key(idx_->get_key_type(), idx_->get_key_len());
	BTNode *newnode = pnode->split(key);
	idx_->IncreaseNodeCount();
	int parent = pnode->get_parent();

	if (parent == -1)											/* ��ǰ�ڵ��Ǹ��ڵ� */
	{
		BTNode *newroot = new BTNode(this, true, get_new_blocknum());
		if (newroot == NULL) return false;

		idx_->IncreaseNodeCount();
		idx_->set_root(newroot->get_block_num());

		newroot->add(key);
		newroot->set_values(0, pnode->get_block_num());
		newroot->set_values(1, newnode->get_block_num());

		pnode->set_parent(idx_->get_root());
		newnode->set_parent(idx_->get_root());
		newnode->set_next_leaf(-1);
		idx_->IncreaseLevel();
		return true;
	}
	else
	{
		BTNode *parentnode = get_node(parent);
		int index = parentnode->add(key);

		parentnode->set_values(index, pnode->get_block_num());
		parentnode->set_values(index + 1, newnode->get_block_num());

		if (parentnode->get_count() == degree_)
			return spiltForAdd(parentnode->get_block_num());
		return true;
	}
}
/*�Ƴ�keyԪ��*/
bool BPlusTree::remove(TKey key)
{
	if (idx_->get_root() == -1) return false;

	BTNode *rootnode = get_node(idx_->get_root());
	FindNodeParam fnp = search(idx_->get_root(), key);

	if (fnp.flag)
	{
		if (idx_->get_root() == fnp.pnode->get_block_num())
		{
			rootnode->remove(fnp.index);
			idx_->DecreaseKeyCount();
			mergeForRemove(fnp.pnode->get_block_num());
			return true;
		}

		if (fnp.index == fnp.pnode->get_count() - 1)
		{
			FindNodeParam fnpb = search_pos(idx_->get_root(), key);
			if (fnpb.flag)
				fnpb.pnode->set_keys(fnpb.index, fnp.pnode->get_keys(fnp.pnode->get_count() - 2));
		}

		fnp.pnode->remove(fnp.index);
		idx_->DecreaseKeyCount();
		mergeForRemove(fnp.pnode->get_block_num());
		return true;
	}
	return false;
}
/*ɾԪ�غ����B+�����ϲ�*/
bool BPlusTree::mergeForRemove(int node)
{
	BTNode* pnode = get_node(node);
	if (pnode->get_count() >= idx_->get_rank()) return true;

	if (pnode->isRoot())
	{
		if (pnode->get_count() == 0)
		{
			if (!pnode->is_leaf())
			{
				idx_->set_root(pnode->get_values(0));
				get_node(pnode->get_values(0))->set_parent(-1);
			}
			else
			{
				idx_->set_root(-1);
				idx_->set_leaf_head(-1);
			}
			delete pnode;
			idx_->DecreaseNodeCount();
			idx_->DecreaseLevel();
		}
		return true;
	}

	BTNode *pbrother;
	BTNode *pparent;
	int pos;

	pparent = get_node(pnode->get_parent());
	pparent->search(pnode->get_keys(0), pos);/*�ڸ��ڵ��в�ѯָ��pnode��ָ���ַpos*/

	if (pos == pparent->get_count())/*��pnode�Ǹ��ڵ�����һ��*/
	{
		pbrother = get_node(pparent->get_values(pos - 1));/*�ֵܽڵ�:���ڵ��ǰһ���ڵ�*/
		if (pbrother->get_count() > idx_->get_rank())/*����ֵܽڵ�������������涨��rank����pbrother�����һ��Ԫ������pnode*/
		{
			if (pnode->is_leaf())/*pnodeΪҶ�ӽڵ�*/
			{
				for (int i = pnode->get_count(); i > 0; i--)/*pnode��ÿһ��Ԫ�غ���*/
				{
					pnode->set_keys(i, pnode->get_keys(i - 1));/*ָ����һ��Ҷ�ӽڵ�ĵ�ַָ�����blockͷ������������Ҷ�ӽڵ��key��=value��*/
					pnode->set_values(i, pnode->get_values(i - 1));
				}
				pnode->set_keys(0, pbrother->get_keys(pbrother->get_count() - 1));/*��pbrother�����һ��Ԫ������pnode����Ԫ��λ��*/
				pnode->set_values(0, pbrother->get_values(pbrother->get_count() - 1));
				pnode->set_count(pnode->get_count() + 1);

				pbrother->set_count(pbrother->get_count() - 1);
				pparent->set_keys(pos - 1, pbrother->get_keys(pbrother->get_count() - 1));/*��Ҷ�ӽڵ���һ���key�ı仯�����ø��ڵ�������pbrother��pnode��keyΪpbrother�����һ��Ԫ�ص�key���������ַ�ʽΪ��<= �� > */
				return true;
			}
			else
			{
				/*pnodeԪ�غ���һλ*/
				for (int i = pnode->get_count(); i > 0; i--)
					pnode->set_keys(i, pnode->get_keys(i - 1));
				for (int i = pnode->get_count() + 1; i > 0; i--)
					pnode->set_values(i, pnode->get_values(i - 1));
				//

				pnode->set_keys(0, pparent->get_keys(pos - 1));
				pparent->set_keys(pos - 1, pbrother->get_keys(pbrother->get_count() - 1));

				pnode->set_values(0, pbrother->get_values(pbrother->get_count()));
				pnode->set_count(pnode->get_count() + 1);

				if (pbrother->get_values(pbrother->get_count()) >= 0)
				{
					get_node(pbrother->get_values(pbrother->get_count()))->set_parent(pnode->get_block_num());
					pbrother->set_values(pbrother->get_count(), -1);
				}
				pbrother->set_count(pbrother->get_count() - 1);
				return true;
			}
		}
		else/*pnodeԪ�ع��٣��ϲ���pbrother*/
		{
			if (pnode->is_leaf())
			{
				pparent->remove(pos - 1);
				pparent->set_values(pos - 1, pbrother->get_block_num());

				for (int i = 0; i < pnode->get_count(); i++)
				{
					pbrother->set_keys(pbrother->get_count() + i, pnode->get_keys(i));
					pbrother->set_values(pbrother->get_count() + i, pnode->get_values(i));
					pnode->set_values(i, -1);
				}
				pbrother->set_count(pbrother->get_count() + pnode->get_count());
				pbrother->set_next_leaf(pnode->get_next_leaf());

				delete pnode;
				idx_->DecreaseNodeCount();
				return mergeForRemove(pparent->get_block_num());
			}
			else
			{
				pbrother->set_keys(pbrother->get_count(), pparent->get_keys(pos - 1));
				pbrother->set_count(pbrother->get_count() + 1);
				pparent->remove(pos - 1);
				pparent->set_values(pos - 1, pbrother->get_block_num());

				for (int i = 0; i < pnode->get_count(); i++)
					pbrother->set_keys(pbrother->get_count() + i, pnode->get_keys(i));

				for (int i = 0; i <= pnode->get_count(); i++)
				{
					pbrother->set_values(pbrother->get_count() + i, pnode->get_values(i));
					get_node(pnode->get_values(i))->set_parent(pbrother->get_block_num());
				}
				pbrother->set_count(2 * idx_->get_rank());

				delete pnode;
				idx_->DecreaseNodeCount();
				return mergeForRemove(pparent->get_block_num());
			}
		}
	}
	else/*pnode�����Ǹ��ڵ�����һ���ڵ㣬��pnode���ֵܽڵ��Ǻ�һ���ڵ�*/
	{
		pbrother = get_node(pparent->get_values(pos + 1));
		if (pbrother->get_count() > idx_->get_rank())
		{
			if (pnode->is_leaf())
			{
				pparent->set_keys(pos, pbrother->get_keys(0));
				pnode->set_keys(pnode->get_count(), pbrother->get_keys(0));
				pnode->set_values(pnode->get_count(), pbrother->get_values(0));

				pbrother->set_values(0, -1);
				pnode->set_count(pnode->get_count() + 1);
				pbrother->remove(0);
				return true;
			}
			else
			{
				pnode->set_keys(pnode->get_count(), pparent->get_keys(pos));
				pnode->set_values(pnode->get_count() + 1, pbrother->get_values(0));
				pnode->set_count(pnode->get_count() + 1);
				pparent->set_keys(pos, pbrother->get_keys(0));
				get_node(pbrother->get_values(0))->set_parent(pnode->get_block_num());

				pbrother->remove(0);
				return true;
			}
		}
		else
		{
			if (pnode->is_leaf())
			{
				for (int i = 0; i <idx_->get_rank(); i++)
				{
					pnode->set_keys(pnode->get_count() + i, pbrother->get_keys(i));
					pnode->set_values(pnode->get_count() + i, pbrother->get_values(i));
					pbrother->set_values(i, -1);
				}

				pnode->set_count(pnode->get_count() + idx_->get_rank());
				delete pbrother;
				idx_->DecreaseNodeCount();

				pparent->remove(pos);
				pparent->set_values(pos, pnode->get_block_num());
				return mergeForRemove(pparent->get_block_num());
			}
			else
			{
				pnode->set_keys(pnode->get_count(), pparent->get_keys(pos));
				pparent->remove(pos);
				pparent->set_values(pos, pnode->get_block_num());
				pnode->set_count(pnode->get_count() + 1);

				for (int i = 0; i < idx_->get_rank(); i++)
					pnode->set_keys(pnode->get_count() + i, pbrother->get_keys(i));

				for (int i = 0; i <= idx_->get_rank(); i++)
				{
					pnode->set_values(pnode->get_count() + i, pbrother->get_values(i));
					get_node(pbrother->get_values(i))->set_parent(pnode->get_block_num());
				}

				pnode->set_count(pnode->get_count() + idx_->get_rank());
				delete pbrother;
				idx_->DecreaseNodeCount();
				return mergeForRemove(pparent->get_block_num());
			}
		}
	}
}

/**������ֱ��Ҷ�ӽڵ�Ĳ�ѯ����node��ʼ������key���ڵ�Ҷ�ӽڵ㡣ans.flag��true����key��B+���д��ڣ�false����key��B+���в�����*/
FindNodeParam BPlusTree::search(int node, TKey &key)
{
	FindNodeParam ans;
	int index = 0;
	BTNode* pnode = get_node(node);
	if (pnode->get_count() == 0 && pnode->is_leaf() == false) {
		ans.flag = false;
		ans.index = index;
		ans.pnode = get_node(0);
		return ans;
	}
	if (pnode->search(key, index))
	{
		if (pnode->is_leaf())
		{
			ans.flag = true;
			ans.index = index;
			ans.pnode = pnode;
		}
		else
		{
			pnode = get_node(pnode->get_values(index));
			while (!pnode->is_leaf())
			{
				pnode = get_node(pnode->get_values(pnode->get_count()));
			}
			ans.flag = true;
			ans.index = pnode->get_count() - 1;
			ans.pnode = pnode;
		}
	}
	else
	{
		if (pnode->is_leaf())
		{
			ans.flag = false;
			ans.index = index;
			ans.pnode = pnode;
		}
		else
		{
			return search(pnode->get_values(index), key);
		}
	}
	return ans;
}
/*������ɾ�ڵ�ʱ���ڲ��ڵ�仯�Ĳ�ѯ����node��ʼ����ѯkey���ڵ�pnode����index��FindNodeParam�е�flag��true��pnodeΪҶ�ӽڵ㣻false��pnodeΪ�ڲ��ڵ�*/
FindNodeParam BPlusTree::search_pos(int node, TKey &key)
{
	FindNodeParam ans;
	int index = 0;
	BTNode* pnode = get_node(node);

	if (pnode->is_leaf()) throw BPlusTreeException();
	if (pnode->search(key, index))
	{
		ans.flag = true;
		ans.index = index;
		ans.pnode = pnode;
	}
	else
	{
		if (!get_node(pnode->get_values(index))->is_leaf())
			ans = search_pos(pnode->get_values(index), key);
		else
		{
			ans.flag = false;
			ans.index = index;
			ans.pnode = pnode;
		}
	}
	return ans;
}
/*��ȡ��num���ڵ㡣ʵ�ַ�������ȡ�ļ�ϵͳ�е�num���飬�ÿ��е�data_���ݼ�Ϊ�ýڵ�*/
BTNode* BPlusTree::get_node(int num)
{
	BTNode* pnode = new BTNode(this, false, num);
	return pnode;
}
/*��key��ѯvalueֵ*/
int BPlusTree::get_value(TKey key)
{
	int ans = -1;
	if (idx_->get_root() != -1) {	/*������Ϊ��ʱ������ձ��������ֵ�쳣����*/
		FindNodeParam fnp = search(idx_->get_root(), key);
		if (fnp.flag)
			ans = fnp.pnode->get_values(fnp.index);
	}
	return ans;
}

/*��key��ѯvalueֵ��֧�ֵ�ֵ��ѯ�뷶Χ��ѯ��op_type�������ж��Ƿ�Ϊ��Χ��ѯ*/
vector<int> BPlusTree::get_value(TKey key, int op_type, string & searchType)
{
	vector<int> ans;
	if (idx_->get_root() != -1) {	/*������Ϊ��ʱ������ձ��������ֵ�쳣����*/
		FindNodeParam fnp = search(idx_->get_root(), key);

		BTNode * temp = new BTNode(*fnp.pnode);
		BTNode * firstNode = new BTNode(*get_node(idx_->get_leaf_head()));//��ȡҶ��ͷ���ڵ���Ϣ
		int index = fnp.index;
		switch (op_type)
		{
		case SIGN_EQ:
			if (fnp.flag) {
				ans.push_back(fnp.pnode->get_values(fnp.index));
			}
			break;
		case SIGN_GT://>	
			if (!fnp.flag&&fnp.pnode->get_keys(fnp.pnode->get_count() - 1) <= key) {//��ֹ������������ԣ�id(1)>0�����Ըýڵ������ֵҲ<=keyʱ���ýڵ����Ч��
				break;
			}
			searchType = "����B+���ķ�Χ��ѯ";
			//�����ڵ����Ч��¼				
			while (index < (*temp).get_count()) {
				ans.push_back(temp->get_values(index));
				index++;
			}
			//�������ڵ����Ч��¼
			while ((*temp).get_next_leaf() != -1) {
				temp = get_node((*temp).get_next_leaf());
				index = 0;
				while (index < (*temp).get_count()) {
					ans.push_back(temp->get_values(index));
					index++;
				}
			}
			break;
		case SIGN_LT://<	
			if (!fnp.flag&&fnp.pnode->get_keys(0) >= key) {
				break;
			}
			searchType = "����B+���ķ�Χ��ѯ";
			//��ǰ�ڵ㵽��ǰ�ڵ�ĵ�ǰλ�õ�������Ч��¼

			while ((*firstNode).get_block_num() != (*temp).get_next_leaf()) {
				int idx = 0;
				bool flag = true;
				while (idx < (*firstNode).get_count()) {
					if ((*firstNode).get_block_num() == (*temp).get_block_num() && idx == index)//������ǰ��ĵ�ǰKV��ʱ��ֹͣ
					{
						flag = false;
						break;
					}
					ans.push_back(firstNode->get_values(idx));
					idx++;
				}
				if (!flag)
					break;
				firstNode = get_node((*firstNode).get_next_leaf());//��һ������Ҷ�ӽڵ�
			}
			break;
		case SIGN_GE://>=
			if (!fnp.flag&&fnp.pnode->get_keys(fnp.pnode->get_count() - 1) < key) {
				break;
			}
			searchType = "����B+���ķ�Χ��ѯ";
			ans.push_back(fnp.pnode->get_values(fnp.index));//=
															//�����ڵ����Ч��¼				
			while (++index < (*temp).get_count()) {
				ans.push_back(temp->get_values(index));
			}
			//�������ڵ����Ч��¼
			while ((*temp).get_next_leaf() != -1) {
				temp = get_node((*temp).get_next_leaf());
				index = 0;
				while (index < (*temp).get_count()) {
					ans.push_back(temp->get_values(index));
					index++;
				}
			}
			break;
		case SIGN_LE://<=
			if (!fnp.flag&&fnp.pnode->get_keys(0) > key) {
				break;
			}
			searchType = "����B+���ķ�Χ��ѯ";
			//��ǰ�ڵ㵽��ǰ�ڵ�ĵ�ǰλ�õ�������Ч��¼

			while ((*firstNode).get_block_num() != (*temp).get_next_leaf()) {
				int idx = 0;
				bool flag = true;
				while (idx < (*firstNode).get_count()) {
					if ((*firstNode).get_block_num() == (*temp).get_block_num() && idx == index)
					{
						ans.push_back(fnp.pnode->get_values(fnp.index));//=
						flag = false;
						break;
					}
					ans.push_back(firstNode->get_values(idx));
					idx++;
				}
				if (!flag)
					break;
				firstNode = get_node((*firstNode).get_next_leaf());//��һ������Ҷ�ӽڵ�
			}
			break;
		default:
			break;
		}


	}
	return ans;
}
/*��ȡ��block��ţ�idx_�������ֵ��һ*/
int BPlusTree::get_new_blocknum()
{
	return idx_->IncreaseMaxCount();
}

void BPlusTree::print()
{
	printf("*----------------------------------------------*\n");
	printf("����: %d, �ڵ���: %d, �㼶: %d, ���ڵ�: %d \n", idx_->get_key_count(), idx_->get_node_count(), idx_->get_level(), idx_->get_root());

	if (idx_->get_root() != -1)
		print_node(idx_->get_root());
}
/**��ӡ�ڵ���Ϣ*/
void BPlusTree::print_node(int num)
{
	BTNode* pnode = get_node(num);
	pnode->print();
	if (!pnode->is_leaf())
	{
		for (int i = 0; i <= pnode->get_count(); ++i)
			print_node(pnode->get_values(i));
	}
}
