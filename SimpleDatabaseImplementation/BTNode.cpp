//��飺B+������
//Implemented by Xu Jing
//���ã�ʵ�����B+����
#include <iomanip>
#include<iostream>
#include "BTNode.h"
#include "Exceptions.h"
#include "ConstValue.h"

using namespace std;

BTNode::BTNode(BPlusTree* tree, bool isnew, int blocknum, bool newleaf)
{
	tree_ = tree;
	is_leaf_ = newleaf;
	rank_ = (tree_->get_degree() - 1) / 2;
	block_num_ = blocknum;
	get_buffer();
	if (isnew)
	{
		set_parent(-1);
		set_node_type(newleaf ? 1 : 0);
		set_count(0);
	}
}

BTNode::~BTNode() {}

int BTNode::get_block_num() { return block_num_; }
/*��ȡ��index��keyֵ*/
TKey BTNode::get_keys(int index)
{
	TKey k(tree_->GetIndex()->get_key_type(), tree_->GetIndex()->get_key_len());
	int base = 12;
	int lenr = 4 + tree_->GetIndex()->get_key_len();
	memcpy(k.get_key(), &buffer_[base + index * lenr + 4], tree_->GetIndex()->get_key_len());
	return k;
}
/*��ȡ��index��valueֵ*/
int BTNode::get_values(int index)
{
	int base = 12;
	int lenR = 4 + tree_->GetIndex()->get_key_len();
	return *((int*)(&buffer_[base + index*lenR]));
}
/*��ȡ��һ��Ҷ�ӽڵ�*/
int BTNode::get_next_leaf()
{
	int base = 12;
	int lenR = 4 + tree_->GetIndex()->get_key_len();
	return *((int*)(&buffer_[base + tree_->get_degree()*lenR]));
}
/*��ȡ��һ�����ڵ㣬�õ����Ǹýڵ���buffer�еĵ�ַ��ţ��Ǹýڵ����ڵ�buffer�ĵ�8-11�ֽ�����*/
int BTNode::get_parent() { return *((int*)(&buffer_[8])); }
/*��ȡ�ڵ����ͣ����ýڵ�����buffer�еĵ�0-3�ֽ�����*/
int BTNode::get_node_type() { return *((int*)(&buffer_[0])); }
/*��ȡ�ڵ�����ݸ��������ýڵ�����buffer�еĵ�4-7�ֽ�����*/
int BTNode::get_count() { return *((int*)(&buffer_[4])); }
/*�ж��Ƿ���Ҷ�ӽڵ�*/
bool BTNode::is_leaf() { return get_node_type() == 1; }
/*��key.key_����ýڵ�ĵ�index������*/
void BTNode::set_keys(int index, TKey key)
{
	int base = 12;
	int lenr = 4 + tree_->GetIndex()->get_key_len();
	memcpy(&buffer_[base + index*lenr + 4], key.get_key(), tree_->GetIndex()->get_key_len());
}
/*���õ�indexԪ�ص�ֵΪval*/
void BTNode::set_values(int index, int val)
{
	int base = 12;
	int lenr = 4 + tree_->GetIndex()->get_key_len();
	*((int*)(&buffer_[base + index*lenr])) = val;
}
/*������һ��Ҷ�ӽڵ��ֵ*/
void BTNode::set_next_leaf(int val)
{
	int base = 12;
	int len = 4 + tree_->GetIndex()->get_key_len();
	*((int*)(&buffer_[base + tree_->get_degree() * len])) = val;/*���øýڵ�����һ��valueΪָ����һҶ�ӽڵ��ָ�롣degree���ڵ�Ķȣ����ýڵ���Էŵ�KV��������*/
}
/*���ø��ڵ��ֵ*/
void BTNode::set_parent(int val) { *((int*)(&buffer_[8])) = val; }
/*�ڵ����ͣ�Ҷ�ӽڵ�Ϊ1����Ҷ�ӽڵ�Ϊ0*/
void BTNode::set_node_type(int val) { *((int*)(&buffer_[0])) = val; }
/*���ýڵ�洢��Ԫ�ظ���*/
void BTNode::set_count(int val) { *((int*)(&buffer_[4])) = val; }
/*���ýڵ��Ƿ�ΪҶ�ӽڵ�*/
void BTNode::set_is_leaf(bool val) { set_node_type(val ? 1 : 0); }
/*��file�л�ȡ��ǰB+�����ڵ�db�е�ǰ���������ļ���*/
void BTNode::get_buffer()
{
	BlockInfo *bp = tree_->GetBufferManager()->GetFileBlock(tree_->get_db_name(), tree_->GetIndex()->get_name(), FORMAT_INDEX, block_num_);
	buffer_ = bp->get_data();
	bp->set_dirty(true);
}
/*��B+��������key���ڵ�λ�ã�����ֵ��index�С�����ֵ��true��index��Ϊ��key��λ�Ľڵ��ַ��false��index��Ϊָ����һ���ָ�롣�ڵ�Ԫ�ظ���20���ڣ�˳����ң�20���⣬���ֲ���*/
bool BTNode::search(TKey key, int &index)
{
	bool ans = false;
	if (get_count() == 0) { index = 0; return false; }
	if (get_keys(0) > key) { index = 0;  return false; }
	if (get_keys(get_count() - 1) < key) { index = get_count(); return false; }
	if (get_count() > 20)										/* ���ֲ��� */
	{
		int mid, start, end;
		start = 0;
		end = get_count() - 1;
		while (start < end)
		{
			mid = (start + end) / 2;
			if (key == get_keys(mid)) { index = mid; return true; }
			else if (key < get_keys(mid)) end = mid;
			else start = mid;

			if (start == end - 1)
			{
				if (key == get_keys(start)) { index = start; return true; }
				if (key == get_keys(end)) { index = end; return true; }
				if (key < get_keys(start)) { index = start; return false; }
				if (key < get_keys(end)) { index = end; return false; }
				if (key > get_keys(end)) { index = end + 1; return false; }
			}
		}
		return false;
	}
	else														/* ˳����� */
	{
		for (int i = 0; i < get_count(); i++)
		{
			if (key < get_keys(i))
			{
				index = i;
				ans = false;
				break;
			}
			else if (key == get_keys(i))
			{
				index = i;
				ans = true;
				break;
			}
		}
		return ans;
	}
}
/*�Ȳ���b+�����Ƿ���ڸ�key���񣺽�key�������Ӧ��λ����*/
int BTNode::add(TKey &key)
{
	int index = 0;
	if (get_count() == 0)
	{
		set_keys(0, key);
		set_count(1);
		return 0;
	}
	if (!search(key, index))
	{
		for (int i = get_count(); i > index; i--)
			set_keys(i, get_keys(i - 1));

		for (int i = get_count() + 1; i > index; i--)
			set_values(i, get_values(i - 1));

		set_keys(index, key);
		set_values(index, -1);
		set_count(get_count() + 1);
	}
	return index;
}
/*����KV��*/
int BTNode::add(TKey &key, int &val)
{
	int index = 0;
	if (get_count() == 0)
	{
		set_keys(0, key);
		set_values(0, val);
		set_count(get_count() + 1);
		return 0;
	}
	if (!search(key, index)) {
		for (int i = get_count(); i > index; i--)
		{
			set_keys(i, get_keys(i - 1));
			set_values(i, get_values(i - 1));
		}

		set_keys(index, key);
		set_values(index, val);
		set_count(get_count() + 1);
	}
	return index;
}
/*�ڵ����*/
BTNode* BTNode::split(TKey &key)
{
	BTNode* newnode = new BTNode(tree_, true, tree_->get_new_blocknum(), is_leaf());
	if (newnode == NULL)
	{
		throw BPlusTreeException();
		return NULL;
	}
	key = get_keys(rank_);
	if (is_leaf())
	{
		for (int i = rank_ + 1; i< tree_->get_degree(); i++)
		{
			newnode->set_keys(i - rank_ - 1, get_keys(i));
			newnode->set_values(i - rank_ - 1, get_values(i));
		}

		newnode->set_count(rank_);
		set_count(rank_ + 1);
		newnode->set_next_leaf(get_next_leaf());
		set_next_leaf(newnode->get_block_num());
		newnode->set_parent(get_parent());
	}
	else
	{
		for (int i = rank_ + 1; i< tree_->get_degree(); i++)
			newnode->set_keys(i - rank_ - 1, get_keys(i));

		for (int i = rank_ + 1; i <= tree_->get_degree(); i++)
			newnode->set_values(i - rank_ - 1, get_values(i));

		newnode->set_parent(get_parent());
		newnode->set_count(rank_);

		int childnode_num;
		for (int i = 0; i <= newnode->get_count(); i++)
		{
			childnode_num = newnode->get_values(i);
			BTNode* node = tree_->get_node(childnode_num);
			if (node) node->set_parent(newnode->get_block_num());
		}
		set_count(rank_);
	}
	return newnode;
}
/*�ж��Ƿ�Ϊ���ڵ�*/
bool BTNode::isRoot()
{
	if (get_parent() == -1) return true;
	return false;
}
//�ӱ��ڵ���ɾ����indexԪ��
bool BTNode::remove(int index)
{
	if (index > get_count() - 1) return false;
	if (is_leaf()) {
		for (int i = index; i < get_count() - 1; i++)
		{
			set_keys(i, get_keys(i + 1));
			set_values(i, get_values(i + 1));
		}
	}
	else
	{
		for (int i = index; i< get_count() - 1; i++)
			set_keys(i, get_keys(i + 1));

		for (int i = index; i < get_count(); i++)
			set_values(i, get_values(i + 1));
	}
	set_count(get_count() - 1);
	return true;
}
//B+���ڵ��ӡ
void BTNode::print()
{
	printf("*----------------------------------------------*\n");
	printf("���: %d ����: %d, ���ڵ�: %d  ��Ҷ�ӽڵ㣿:%d\n", block_num_, get_count(), get_parent(), is_leaf());
	printf("��K: { ");
	for (int i = 0; i < get_count(); i++)
		cout << setw(9) << left << get_keys(i);
	printf(" }\n");

	if (is_leaf())
	{
		printf("ֵV: { ");
		for (int i = 0; i < get_count(); i++)
		{
			if (get_values(i) == -1) printf("{NUL}");
			else
				cout << setw(9) << left << get_values(i);
		}
		printf(" }\n");
		printf("��һҶ��: %5d\n", get_next_leaf());
	}
	else
	{
		printf("Ptrs: {");
		for (int i = 0; i <= get_count(); i++)
			cout << setw(9) << left << get_values(i);

		printf("}\n");
	}
}
