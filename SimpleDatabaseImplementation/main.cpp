#include"QueryParser.h"
#include<iostream>
using namespace std;
int main()
{
	string tmp;
	QueryParser t;
	bool flag = true;//�Ƿ�Ҫ�������ɹ���flag
	t.ExecuteSQL("help;", flag);
	while (getline(cin, tmp))
	{
		if (tmp.find(';') != string::npos)
		{
			t.ExecuteSQL(tmp, flag);
		}
		else
		{
			cout << "\'" + tmp + "\'" + "������ȷ�����������help�鿴������" << endl;
		}
	}
}