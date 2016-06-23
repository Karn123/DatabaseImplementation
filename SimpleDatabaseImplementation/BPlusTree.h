//��飺B+������
//Implemented by Xu Jing
//���ã�ʵ�����B+����
#pragma once
#ifndef _BPLUSTREE_H_
#define _BPLUSTREE_H_

#include <string>
#include <vector>

#include "BTNode.h"
#include "CatalogManager.h"
#include "BufferManager.h"

using namespace std;

class BPlusTree;		/*B+����*/
class BTNode;

typedef struct
{
	BTNode* pnode;
	int index;
	bool flag;
} FindNodeParam;				/*�ڵ���Ҹ����ṹ*/

class BPlusTree
{
public:
	BPlusTree(Index* idx, BufferManager* bm, CatalogManager* cm, string dbname);
	~BPlusTree(void);

	Index* GetIndex();										/*��ȡ��B+����������Ա*/
	BufferManager* GetBufferManager();						/*��ȡ��B+����buffermanager*/
	CatalogManager* GetCatalogManager();					/*��ȡ��B+����Ŀ¼������*/
	int get_degree();										/*��ȡ��*/
	string get_db_name();									/*��ȡ��B+�������ݿ���*/

	bool add(TKey& key, int block_num, int offset);			/*�ڵ�block_num�����ϼ�Ԫ��key ƫ��offset*/
	bool spiltForAdd(int node);							/*��Ԫ�غ����B+��*/

	bool remove(TKey key);									/*�Ƴ�keyԪ��*/
	bool mergeForRemove(int node);						/*ɾԪ�غ����B+��*/

	FindNodeParam search(int node, TKey &key);				/*������ֱ��Ҷ�ӽڵ�Ĳ�ѯ����node��ʼ������key���ڵ�Ҷ�ӽڵ㡣ans.flag��true����key��B+���д��ڣ�false����key��B+���в�����*/
	FindNodeParam search_pos(int node, TKey &key);		/*������ɾ�ڵ�ʱ���ڲ��ڵ�仯�Ĳ�ѯ����node��ʼ����ѯkey���ڵ�pnode����index��FindNodeParam�е�flag��true��pnodeΪҶ�ӽڵ㣻false��pnodeΪ�ڲ��ڵ�*/
	BTNode* get_node(int num);						/*��ȡ��num���ڵ㡣ʵ�ַ�������ȡ�ļ�ϵͳ�е�num���飬�ÿ��е�data_���ݼ�Ϊ�ýڵ�*/

	int get_value(TKey key);									/*��key��ѯvalueֵ*/
	vector<int> get_value(TKey key, int op_type, string &searchType);		/*��key��ѯvalueֵ��op_type�������ж��Ƿ�Ϊ��Χ��ѯ*/
	int get_new_blocknum();									/*idx_�������ֵ��һ*/

	void print();
	void print_node(int num);

private:
	Index *idx_;											/*����ָ�룬�����˵�ǰB+���Ļ�����Ϣ��ͷ�ڵ��ַ�����Լ����ȵȣ�����һ��table������һ��B+�����������table����������Ϣ���������б��У����������ָ����table�е������б��ĳһ�*/
	int degree_;											/*��*/
	BufferManager *buffer_m_;								/*���������ָ��*/
	CatalogManager *catalog_m_;								/*Ŀ¼������ָ��*/
	string db_name_;										/*��B+��������db����*/
	void InitTree();										/*��ʼ�����������ڵ㣬��ʼidx����*/
};

#endif