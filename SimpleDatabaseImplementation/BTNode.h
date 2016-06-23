#pragma once
//��飺B+���ڵ���
//Implemented by Xu Jing
//���ã�ʵ�����B+�����ڵ�
#pragma once
#ifndef _BTNODE_H_
#define _BTNODE_H_

#include <string>
#include <vector>

#include "BPlusTree.h"
#include "CatalogManager.h"
#include "BufferManager.h"

using namespace std;

class BPlusTree;
class BTNode
{
public:
	BTNode(BPlusTree* tree, bool isnew, int blocknum, bool newleaf = false);
	~BTNode();

	int get_block_num();

	TKey get_keys(int index);			/*��ȡ��index��keyֵ*/
	int get_values(int index);			/*��ȡ��index��valueֵ*/
	int get_next_leaf();					/*��ȡ��һ��Ҷ�ӽڵ�*/
	int get_parent();					/*��ȡ��һ�����ڵ㣬�õ����Ǹýڵ���buffer�еĵ�ַ��ţ��Ǹýڵ����ڵ�buffer�ĵ�8λ����*/
	int get_node_type();					/*��ȡ�ڵ����ͣ����ýڵ�����buffer�еĵ�0λ����*/
	int get_count();						/*��ȡ�ڵ�����ݸ��������ýڵ�����buffer�еĵ�4λ����*/
	bool is_leaf();					/*�ж��Ƿ���Ҷ�ӽڵ�*/

	void set_keys(int index, TKey key);	/*��key.key_����ýڵ�ĵ�index������*/
	void set_values(int index, int val);	/*���õ�indexԪ�ص�ֵΪval*/
	void set_next_leaf(int val);			/*������һ��Ҷ�ӽڵ��ֵ*/
	void set_parent(int val);			/*���ø��ڵ��ֵ*/
	void set_node_type(int val);			/*�ڵ����ͣ�Ҷ�ӽڵ�Ϊ1����Ҷ�ӽڵ�Ϊ0*/
	void set_count(int val);				/*���ýڵ�洢��Ԫ�ظ���*/
	void set_is_leaf(bool val);			/*���ýڵ��Ƿ�ΪҶ�ӽڵ�*/

	void get_buffer();					/*��file�л�ȡ��ǰB+�����ڵ�db�е�ǰ���������ļ���*/

	bool search(TKey key, int &index);	/*��B+��������key���ڵ�λ�ã�����ֵ��index�С�����ֵ��true��index��Ϊ��key��λ�Ľڵ��ַ��false��index��Ϊָ����һ���ָ�롣�ڵ�Ԫ�ظ���20���ڣ�˳����ң�20���⣬���ֲ���*/
	int add(TKey &key);					/*�Ȳ���b+�����Ƿ���ڸ�key���񣺽�key�������Ӧ��λ����*/
	int add(TKey &key, int &val);		/*����KV��*/
	BTNode* split(TKey &key);	/*����*/

	bool isRoot();						/*�ж��Ƿ�Ϊ���ڵ�*/
	bool remove(int index);			/*�Ƴ���index��Ԫ��*/
	void print();

private:
	BPlusTree* tree_;
	int block_num_;
	int rank_;
	char* buffer_;//һ�������block��
	bool is_leaf_;
	bool is_new_node_;
};
#endif