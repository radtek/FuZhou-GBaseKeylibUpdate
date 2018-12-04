#pragma once
#include "acl_cpp/lib_acl.hpp"

#define _MYSQL_SELECT 1 //select���
#define _MYSQL_INSERT 2 //insert���
#define _MYSQL_DELETE 3 //delete���
#define _MYSQL_UPDATA 4 //updata���

#ifdef _DEBUG
#pragma comment( lib, "lib_acl_cpp_vc2010d.lib" )
#pragma comment( lib, "lib_acl_vc2010d.lib" )
#else
#pragma comment( lib, "lib_acl_cpp_vc2010.lib" )
#pragma comment( lib, "lib_acl_vc2010.lib" )
#endif



class CMysql_acl
{
public:
	CMysql_acl(void);
	~CMysql_acl(void);

	/************************************************* 
	@Input : 
	[sIP]:    ��������ַ
	[nPort]:  �������˿�
	[dbName]: ���ݿ���
	[sUser]:  �û���
	[sPwd]:   ����
	[charset]:����ʹ�õ��ַ���;utf8��gb2312��GBK��.
	@Output:  NULL
	@Return:  ����<0ʧ��;select��䷵�ؼ�¼��;��������Ӱ������
	@Others: 
	*************************************************/  
	bool        mysql_connectDB(const char* sIP, int nPort, const char* dbName, const char* sUser, const char* sPwd, const char* charset);
	void        mysql_disconnectDB();

	/************************************************* 
	@Input : 
	[nSqlType]: sql�������, �鿴�궨��
    [sql]: ִ�����,֧�ֱ��
	@Output: NULL
	@Return: ����<0ʧ��;select��䷵�ؼ�¼��;��������Ӱ������
	@Others:
	*************************************************/  
	int         mysql_exec(int nSqlType, const char* sql, ...);
	

	/************************************************* 
	@Input : 
	[colName]: ����
	@Output: NULL
	@Return: һ������ָ���е�ֵ
	@Others: ѭ������mysql_getNextRow()��ȡÿһ������
	*************************************************/  
	bool        mysql_getNextRow();
	int         mysql_getRowIntValue(const char* colName);
	double      mysql_getRowDoubleValue(const char* colName);
	const char* mysql_getRowStringValue(const char* colName);

private:
	bool        mysql_Init();
	int         mysql_Insert(acl::db_handle& h_db, const char* sql);
	int         mysql_Delete(acl::db_handle& h_db, const char* sql);
	int         mysql_Updata(acl::db_handle& h_db, const char* sql);
	int         mysql_Select(acl::db_handle& h_db, const char* sql);

	bool        mysql_getNextRow_private(acl::db_handle& h_db);
	int         mysql_getRowIntValue_private(acl::db_handle& h_db, const char* colName);
	double      mysql_getRowDoubleValue_private(acl::db_handle& h_db, const char* colName);
	const char* mysql_getRowStringValue_private(acl::db_handle& h_db, const char* colName);
	

private:
	acl::db_mysql*     m_pdb;
	size_t             m_nCurRow;  //��ѯ�������ݵ�ĳһ����
	size_t             m_nRowCount;//��ѯ�������ݵ�������

};