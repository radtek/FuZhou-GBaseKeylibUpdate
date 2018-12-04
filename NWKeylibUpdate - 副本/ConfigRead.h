#pragma once
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/typeof/typeof.hpp>

using namespace boost::property_tree;
using namespace std;

#pragma comment( lib, "XS_Encrypt.lib" )


class CConfigRead
{
public:
	CConfigRead(void);
public:
    ~CConfigRead(void);
public:
    string GetCurrentPath();
    bool ReadConfig();
public:
    string m_sCurrentPath;  //����ǰ·��
    string m_sConfigFile;	//����·��

    string m_sDBIP;		    //���ݿ�IP
    int m_nDBPort;          //���ݿ�˿�
    string m_sDBName;		//���ݿ���
    string m_sDBUid;		//�û���
    string m_sDBPwd;		//�û�����

    string m_sServerCode;   //����Code
    string m_sSavePath;     //ͼƬ�������
    int m_nQuality;         //���ڴ������������ص��ͼƬ�����

    string m_sOracleIP;
    string m_sOracleName;
    string m_sOracleUser;
    string m_sOraclePassword;

    string m_sBatchStoreServerIP;   //�������ط���ID
    int m_nBatchStoreServerPort;    //�������ط���Port
    
    int m_nWaitTime;                //���ѭ��һ��
};
