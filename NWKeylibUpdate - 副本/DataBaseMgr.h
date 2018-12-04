#pragma once
#define OTL_ORA9I //����oracleʱ
//#define OTL_ODBC // Compile OTL 4/ODBC. Uncomment this when used with MS SQL 7.0/ 2000

#define EXIT_FAILURE_NUM 1
#define RECONNECT_NUM 2

#include "oci/otlv4.h"
#include <string>
using namespace std;

enum Driver { Oracle = 1, SQLServer };
#pragma comment(lib, "oci.lib")


class CDataBaseMgr
{
public:
	CDataBaseMgr(void);
	~CDataBaseMgr(void);
public:

	//����������Ϣ
	void SetConnectString(int nDriver, string sServer, string sDatabase, string sUid, string sPwd);

	//�������ݿ�
	bool ConnectDB();

	//�����ݿ�Ͽ�����
	bool DisconnectDB();

	//�������ݿ�
	bool ReConnectDB();

	//ִ����ɾ�Ĳ����
	bool ExecuteSQL(const char* sql,otl_stream &otlCur, bool bCommit = false);


private:

	string m_sConnection;
	int  m_nDirver;

	int m_nReconnect;   //�������ݿ����

	CRITICAL_SECTION m_clock;
	otl_connect m_db;

    string m_sServer;
    string m_sDateBase;

    time_t m_tConnectDB;
    bool m_bConnectDB;  //��ǰ�������ݿ�״̬, true: ������, false: δ����
public:
    string m_sSIPChannelTable;
    string m_sSIPDVRTable;
};
