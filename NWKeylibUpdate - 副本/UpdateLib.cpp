#include "StdAfx.h"
#include "UpdateLib.h"


CLogRecorder g_LogRecorder;
CUpdateLib::CUpdateLib()
{
    m_pDBInfo = NULL;
    m_hStopEvent = CreateEvent(NULL, true, false, NULL);
}

CUpdateLib::~CUpdateLib()
{
    if (NULL != m_pDBInfo)
    {
        delete m_pDBInfo;
        m_pDBInfo = NULL;
    }
    SetEvent(m_hStopEvent);
    CloseHandle(m_hStopEvent);
}
bool CUpdateLib::StartUpdate()
{
    m_pConfigRead = new CConfigRead;
    string sPath = m_pConfigRead->GetCurrentPath();
    string sConfigPath = sPath + "/Config/NWDQKeylibUpdate_config.properties";
#ifdef _DEBUG
    sConfigPath = "./Config/NWDQKeylibUpdate_config.properties";
#endif
    g_LogRecorder.InitLogger(sConfigPath.c_str(), "NWDQKeylibUpdateLogger", "NWDQKeylibUpdate");

    //读取配置文件
    if (!m_pConfigRead->ReadConfig())
    {
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: 读取配置文件参数错误!");
        return false;
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "读取配置文件成功.");
    }

    m_pDBInfo = new UPDATEDBINFO;
    strcpy(m_pDBInfo->pOracleDBIP, m_pConfigRead->m_sOracleIP.c_str());
    strcpy(m_pDBInfo->pOracleDBName, m_pConfigRead->m_sOracleName.c_str());
    strcpy(m_pDBInfo->pOracleDBUser, m_pConfigRead->m_sOracleUser.c_str());
    strcpy(m_pDBInfo->pOracleDBPassword, m_pConfigRead->m_sOraclePassword.c_str());
    strcpy(m_pDBInfo->pMysqlDBIP, m_pConfigRead->m_sDBIP.c_str());
    strcpy(m_pDBInfo->pMysqlDBName, m_pConfigRead->m_sDBName.c_str());
    strcpy(m_pDBInfo->pMysqlDBUser, m_pConfigRead->m_sDBUid.c_str());
    strcpy(m_pDBInfo->pMysqlDBPassword, m_pConfigRead->m_sDBPwd.c_str());
    m_pDBInfo->nMysqlDBPort = m_pConfigRead->m_nDBPort;
    strcpy(m_pDBInfo->pSavePath, m_pConfigRead->m_sSavePath.c_str());
    strcpy(m_pDBInfo->pBatchStoreServerIP, m_pConfigRead->m_sBatchStoreServerIP.c_str());
    m_pDBInfo->nBatchStoreServerPort = m_pConfigRead->m_nBatchStoreServerPort;

    do 
    {
        //连接Oracle
        m_DBMgr.SetConnectString(Oracle, m_pDBInfo->pOracleDBIP, m_pDBInfo->pOracleDBName, m_pDBInfo->pOracleDBUser, m_pDBInfo->pOracleDBPassword);
        if (!m_DBMgr.ConnectDB())
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error : 连接oracle数据库[%s]失败!", m_pDBInfo->pOracleDBIP);
            return false;
        }
        else
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Success : 连接oracle数据库[%s]成功!", m_pDBInfo->pOracleDBIP);
        }

        //连接MySQL数据库
        if (!m_mysqltool.mysql_connectDB(m_pDBInfo->pMysqlDBIP, m_pDBInfo->nMysqlDBPort, m_pDBInfo->pMysqlDBName,
            m_pDBInfo->pMysqlDBUser, m_pDBInfo->pMysqlDBPassword, "gb2312"))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: 连接Mysql数据库失败[%s:%d:%s]!",
                m_pDBInfo->pMysqlDBIP, m_pDBInfo->nMysqlDBPort, m_pDBInfo->pMysqlDBName);
            return false;
        }
        else
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "连接Mysql数据库成功[%s:%d:%s].",
                m_pDBInfo->pMysqlDBIP, m_pDBInfo->nMysqlDBPort, m_pDBInfo->pMysqlDBName);
        }


        //同步Oracle黑名单库信息到Mysql
        if (!SyncKeyLibInfo())
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error : 同步黑名单库信息失败!");
            return false;
        }
        //下载新增图片到本地保存
        if (!DownloadAddImage())
        {
            g_LogRecorder.WriteDebugLog(__FUNCTION__, "Error: 下载新增图片保存到本地失败!");
            return false;
        }
        //将新增图片信息推送到批量布控服务
        if (!AddBatchInfo())
        {
            return false;
        }
        //删除冗余布控信息到批量布控服务
        if (!DelBatchInfo())
        {
            return false;
        }
        //更新StoreCount表入库图片数量
        if (!UpdateStoreCount())
        {
            return false;
        }
        //清理所有信息, 为下一次更新准备
        if (!ClearInfo())
        {
            return false;
        }
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "更新结束, 等待下一次更新.....");
    } while (WAIT_TIMEOUT == WaitForSingleObject(m_hStopEvent, 1000 *m_pConfigRead->m_nWaitTime));
    
    return true;
}
bool CUpdateLib::StopUpdate()
{
    ClearInfo();
    SetEvent(m_hStopEvent);
    return true;
}
//清理所有信息, 为下一次更新准备
bool CUpdateLib::ClearInfo()
{
    m_nLayoutSuccess = 0;
    m_nLayoutFailed = 0;
    m_mapLayoutBH.clear();
    m_setIgnore.clear();
    MAPZDRYINFO::iterator it = m_mapZDRYInfo.begin();
    while(it != m_mapZDRYInfo.end())
    {
        delete it->second;
        it = m_mapZDRYInfo.erase(it);
    }
    m_DBMgr.DisconnectDB();
    m_mysqltool.mysql_disconnectDB();
    return true;
}
//同步Oracle黑名单库信息到Mysql
bool CUpdateLib::SyncKeyLibInfo()
{
    g_LogRecorder.WriteDebugLog(__FUNCTION__, "开始同步黑名单库信息...");
    int nRet = 0;
    int nNumber = 0;
    char pSql[1024 * 8] = { 0 };
    char pZDRYBH[64] = { 0 };
    char pFaceUUID[64] = { 0 };

    //获取ga_ignore表编号信息，用于插入时判断是否需要插入
    sprintf_s(pSql, sizeof(pSql),
        "Select ztrybh from %s", GAIGNORETABLE);
    nRet = m_mysqltool.mysql_exec(_MYSQL_SELECT, pSql);
    if (nRet > 0)
    {
        while (m_mysqltool.mysql_getNextRow())
        {
            strcpy_s(pZDRYBH, sizeof(pZDRYBH), m_mysqltool.mysql_getRowStringValue("ztrybh"));
            m_setIgnore.insert(pZDRYBH);
        }
        printf("Get Count[%d] from %s..\n", m_setIgnore.size(), GAIGNORETABLE);
    }

    //获取mysql storefaceinfo里所有重点人员信息, 用于后面的更新或插入;
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "获取Mysql数据库黑名单库人员SFZ信息...");
    sprintf_s(pSql, sizeof(pSql),
        "Select zdrybh, faceuuid from %s where LayoutLibId = %d and zdrybh is not null", MYSQLSTOREFACEINFO, DQDDCSTORELIBID);
    nRet = m_mysqltool.mysql_exec(_MYSQL_SELECT, pSql);
    if (nRet > 0)
    {
        while (m_mysqltool.mysql_getNextRow())
        {
            strcpy_s(pZDRYBH, sizeof(pZDRYBH), m_mysqltool.mysql_getRowStringValue("zdrybh"));
            strcpy_s(pFaceUUID, sizeof(pFaceUUID), m_mysqltool.mysql_getRowStringValue("faceuuid"));
            //插入所有正在布控的人员编号信息, 后面判断是否需要删除冗余的数据
            m_mapLayoutBH.insert(make_pair(pZDRYBH, pFaceUUID));
        }
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "获取Mysql数据库盗窃电动车人员编号信息结束, 人员总数[%d].", m_mapLayoutBH.size());

    //生成新增图片文件加地址
    SYSTEMTIME sysTime;
    GetLocalTime(&sysTime);
    char pTime[20] = { 0 };
    sprintf_s(pTime, sizeof(pTime), "%04d%02d%02d%02d%02d%02d/", 
        sysTime.wYear, sysTime.wMonth, sysTime.wDay, sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
    string sZTPath = m_pDBInfo->pSavePath;
    string sZTYXPath = m_pDBInfo->pSavePath + string("DQDDC/"); //在逃有效
    string sZTYXCurTimePath = m_pDBInfo->pSavePath + string("DQDDC/") + string(pTime);  //在逃有效当日更新时间
    CreateDirectory(sZTPath.c_str(), NULL);
    CreateDirectory(sZTYXPath.c_str(), NULL);
    CreateDirectory(sZTYXCurTimePath.c_str(), NULL);
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "创建文件夹[%s]成功.", sZTYXCurTimePath.c_str());

    //获取oracle表黑名单库人员信息(NVL语句是用于有些字段为空时, 读取时会出错, 为空的字段通过NVL指定一个默认值)
    sprintf_s(pSql, sizeof(pSql),
        "select NVL(RYBH, 'null'), NVL(XM, 'null'), NVL(XB, 'null'), "
        "NVL(ZJHM, 'null'), NVL(JZD, 'null'), NVL(AB, 'null') "
        "from %s where RYBH is not NULL and ZP is not NULL", NWORACLEDQDDC);
    otl_stream otlSelect;
    if (!m_DBMgr.ExecuteSQL(pSql, otlSelect))
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "执行SQL语句失败!");
        return false;
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "执行SQL语句, 获取Oracle黑名单库人员信息中...!");
    }
    
    char RYBH[256] = { 0 };
    char XM[256] = { 0 };
    char XB[256] = { 0 };
    char ZJHM[256] = { 0 };
    char JZD[256] = { 0 };
    char AB[256] = { 0 };

    int nAdd = 0;
    while (!otlSelect.eof())
    {
        otlSelect >> RYBH >> XM >> XB >> ZJHM >> JZD >> AB;

        map<string, string>::iterator it = m_mapLayoutBH.find(RYBH);
        //身份证编号己存在则更新, 未存在则插入Mysql同步表
        if (it != m_mapLayoutBH.end())
        {
            m_mapLayoutBH.erase(it);
        }
        else
        {
            set<string>::iterator itIgnore = m_setIgnore.find(RYBH);  //查找确定此编号是否需要忽略
            if (itIgnore != m_setIgnore.end())
            {
                printf("Ignore BH[%s].\n", RYBH);
            }
            else
            {
                nAdd++;
                LPZDRYINFO pZDRYInfo = new ZDRYINFO;
                strcpy(pZDRYInfo->pName, XM);
                if (string(XB) == "女")
                {
                    strcpy(pZDRYInfo->pSex, "2");
                }
                else
                {
                    strcpy(pZDRYInfo->pSex, "1");
                }
                strcpy(pZDRYInfo->pSFZH, ZJHM);
                strcpy(pZDRYInfo->pAddress, JZD);
                strcpy(pZDRYInfo->pAB, AB);
                string sTempPath = sZTYXCurTimePath + string(RYBH) + string(".jpg");
                strcpy(pZDRYInfo->pImagePath, sTempPath.c_str());
                m_mapZDRYInfo.insert(make_pair(RYBH, pZDRYInfo));
            }
        }

        nNumber++;
        if (nNumber % 500 == 0)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "己同步人员信息: %d", nNumber);
        }
    }
    otlSelect.close();

    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "同步黑名单库人员信息表结束, 同步[%d], Add[%d].", nNumber, nAdd);
    return true;
}
//下载新增同步人脸图片到本地保存
bool CUpdateLib::DownloadAddImage()
{
    g_LogRecorder.WriteDebugLog(__FUNCTION__, "开始下载新增人脸图片...");
   
    DWORD dwWrite = 0;
    int nLen = 0;
    otl_stream otlSelect;
    string sTempPath = "";

    CHttpSyncClientPtr pClient(NULL);
    char pSql[1024 * 8] = { 0 };
    char pZPURL[256] = { 0 };
    int nAdd = 0;
    MAPZDRYINFO::iterator it = m_mapZDRYInfo.begin();
    for (; it != m_mapZDRYInfo.end(); it++)
    {
        sprintf_s(pSql, sizeof(pSql),
            "select ZP from %s where RYBH = '%s'", NWORACLEDQDDC, it->first.c_str());
        if (!m_DBMgr.ExecuteSQL(pSql, otlSelect))
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "执行SQL语句失败!");
            return false;
        }

        while (!otlSelect.eof())
        {
            otlSelect >> pZPURL;

            //取图片
            pClient->OpenUrl("GET", pZPURL);

            //收取回应消息
            LPCBYTE pRespon = NULL;
            int nRepSize = 0;
            pClient->GetResponseBody(&pRespon, &nRepSize);

            if(nRepSize > 0)
            {
                HANDLE hFileHandle = CreateFile(it->second->pImagePath,
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL,
                    CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);

                WriteFile(hFileHandle, pRespon, nRepSize, &dwWrite, NULL);
                CloseHandle(hFileHandle);
            }
            else
            {
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "***Warning: Get Image[%s], size = 0.", pZPURL);
            }

            nAdd++;
            if(nAdd % 2000 == 0)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Aleady Download Image: %d.", nAdd);
            }
        }
        otlSelect.close();
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Download Add Image Finished, Count[%d].", nAdd);
    return true;
}
//新增布控信息到批量布控
bool CUpdateLib::AddBatchInfo()
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "增加布控人脸图片信息到批量布控服务, 总数[%d].", m_mapZDRYInfo.size());
    char pHttpURL[128] = { 0 };
    sprintf_s(pHttpURL, sizeof(pHttpURL), "http://%s:%d/Store/addpicture", m_pDBInfo->pBatchStoreServerIP, m_pDBInfo->nBatchStoreServerPort);
    CHttpSyncClientPtr pClient(NULL);

    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType&allocator = document.GetAllocator();

    string sRespon = "";
    char * pImageInfo = new char[MAXIMAGESIZE];
    char * pImageBase64[10];
    for (int i = 0; i < 10; i++)
    {
        pImageBase64[i] = new char[MAXIMAGESIZE];
    }
    DWORD * pRealRead = new DWORD;
    int nFileSize = 0;
    int nLayout = 0;
    m_nLayoutFailed = 0;
    m_nLayoutSuccess = 0;
    MAPZDRYINFO::iterator it = m_mapZDRYInfo.begin();

    while (it != m_mapZDRYInfo.end())
    {
        rapidjson::Value array(rapidjson::kArrayType);
        int i = 0;
        while (it != m_mapZDRYInfo.end())
        {
            if (i < 10)
            {
                //读取图片二进制数据
                HANDLE hFileHandle = CreateFile(it->second->pImagePath,
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
                if (INVALID_HANDLE_VALUE != hFileHandle)
                {
                    nFileSize = GetFileSize(hFileHandle, NULL);
                    if (nFileSize < MAXIMAGESIZE && nFileSize > 0)
                    {
                        ReadFile(hFileHandle, pImageInfo, nFileSize, pRealRead, NULL);

                        //将二进制数据Base64编码, 方便传输
                        string sFeature = ZBase64::Encode((BYTE*)pImageInfo, nFileSize);
                        strcpy(pImageBase64[i], sFeature.c_str());
                        pImageBase64[i][sFeature.size()] = '\0';
                        nLayout ++;

                        //生成Json组数据
                        rapidjson::Value object(rapidjson::kObjectType);
                        object.AddMember("Face", rapidjson::StringRef(pImageBase64[i]), allocator);
                        object.AddMember("Name", rapidjson::StringRef(it->first.c_str()), allocator);
                        array.PushBack(object, allocator);

                        //g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Add Image[%s].", it->second->pImagePath);
                    }
                    else
                    {
                        g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "***Warning: 图片数据过大或为0[%s].", it->second->pImagePath);
                        m_nLayoutFailed ++;
                    }
                    CloseHandle(hFileHandle);
                }
                else
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "open file[%s] failed!", it->second->pImagePath);
                    m_nLayoutFailed ++;
                }
                
                i++;
                it++;
                if (i < 10 && it != m_mapZDRYInfo.end())
                {
                    continue;
                }
            }

            document.AddMember(JSONSTORELIBID, DQDDCSTORELIBID, allocator);
            document.AddMember(JSONLIBTYPE, 3, allocator);
            document.AddMember(JSONSTOREPHOTO, array, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            document.Accept(writer);
            string sDelInfo = string(buffer.GetString());

            //向批量布控服务发送消息
            pClient->OpenUrl("POST", pHttpURL, NULL, 0, (const BYTE*)sDelInfo.c_str(), sDelInfo.size());
            //g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向批量布控服务发送增加布控人脸信息...");

            //收取回应消息
            LPCBYTE pRespon = NULL;
            int nRepSize = 0;
            pClient->GetResponseBody(&pRespon, &nRepSize);
            sRespon.assign((char*)pRespon, nRepSize);
            //g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "批量布控服务回应增加布控人脸信息...");
            //处理回应信息
            InsertAddInfoToDB((char*)sRespon.c_str());

            if(nLayout % 1000 == 0)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, 
                    "Total Count[%d], Handle[%d], Success[%d], Failed[%d].", 
                    m_mapZDRYInfo.size(), nLayout, m_nLayoutSuccess, m_nLayoutFailed);
            }
            i = 0;
            document.RemoveAllMembers();
            break;
        }
    }

    delete[]pImageInfo;
    for (int i = 0; i < 10; i++)
    {
        delete[]pImageBase64[i];
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, 
        "处理新增图片入库结束, Total Count[%d], Handle[%d], Success[%d], Failed[%d].",
        m_mapZDRYInfo.size(), nLayout, m_nLayoutSuccess, m_nLayoutFailed);
    return true;
}
bool CUpdateLib::InsertAddInfoToDB(char * pMsg)
{
    rapidjson::Document document;
    document.Parse(pMsg);
    if (document.HasParseError())
    {
        g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "解析Json串失败[%s]", pMsg);
        return false;
    }

    if (document.HasMember("ErrorMessage") && document["ErrorMessage"].IsString())
    {
        string sErrorMsg = document["ErrorMessage"].GetString();
        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "***Warning: 批量入库服务返回增加人脸错误信息[%s]", sErrorMsg.c_str());
        m_nLayoutFailed += 10;
        return false;
    }
    else
    {
        if (document.HasMember("Photo") && document["Photo"].IsArray() && document["Photo"].Size() > 0)
        {
            char pSQL[8192] = { 0 };
            int nRet = 0;

            //获取本地当前时间
            SYSTEMTIME sysTime;
            GetLocalTime(&sysTime);
            char pTime[32] = { 0 };
            sprintf_s(pTime, sizeof(pTime), "%04d-%02d-%02d %02d:%02d:%02d",
                sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

            for (int i = 0; i < document["Photo"].Size(); i++)
            {
                if (document["Photo"][i].HasMember("Name") && document["Photo"][i]["Name"].IsString() &&
                    document["Photo"][i].HasMember("FaceUUID") && document["Photo"][i]["FaceUUID"].IsString() &&
                    document["Photo"][i].HasMember("SavePath") && document["Photo"][i]["SavePath"].IsString() &&
                    document["Photo"][i].HasMember("face_url") && document["Photo"][i]["face_url"].IsString() &&
                    document["Photo"][i].HasMember("errormessage") && document["Photo"][i]["errormessage"].IsString())
                {
                    string sName = document["Photo"][i]["Name"].GetString();
                    string sFaceUUID = document["Photo"][i]["FaceUUID"].GetString();
                    string sSavePath = document["Photo"][i]["SavePath"].GetString();
                    string sFaceURL = document["Photo"][i]["face_url"].GetString();
                    string sErrorMsg = document["Photo"][i]["errormessage"].GetString();
                    if (sFaceUUID != "")
                    {
                        MAPZDRYINFO::iterator it = m_mapZDRYInfo.find(sName);
                        if (it != m_mapZDRYInfo.end())
                        {
                            sprintf_s(pSQL, sizeof(pSQL),
                                "INSERT into %s(faceuuid, imageid, time, localpath, feature, username, sexradio, idcard, "
                                "address, layoutlibid, controlstatus, updatetime, imageip, face_url, zdrybh, crimetype, crimeaddress) "
                                "VALUES ('%s', '', '%s', '%s', '', '%s', '%s', '%s', "
                                "'%s', %d, 0, '%s', '%s', '%s', '%s', '%s', '%s')",
                                MYSQLSTOREFACEINFO, sFaceUUID.c_str(), pTime, sSavePath.c_str(), it->second->pName, it->second->pSex, it->second->pSFZH,
                                it->second->pAddress, DQDDCSTORELIBID, pTime, m_pDBInfo->pBatchStoreServerIP, sFaceURL.c_str(), it->first.c_str(), it->second->pAB, it->first.c_str());
                            nRet = m_mysqltool.mysql_exec(_MYSQL_INSERT, pSQL);
                            if (nRet < 0)
                            {
                                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "***Warning: 布控成功人脸信息插入到数据库失败.\n%s", pSQL);
                                m_nLayoutFailed ++;
                            }
                            else
                            {
                                m_nLayoutSuccess ++;
                            }
                        }
                    }
                    else
                    {
                        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "[%s]入库失败, ErrorMsg: %s.", sName.c_str(), sErrorMsg.c_str());
                        m_nLayoutFailed ++;
                    }
                }
            }
        }

    }

    return true;
}
//删除冗余布控信息到批量布控服务
bool CUpdateLib::DelBatchInfo()
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "删除冗余布控信息, Count[%d].", m_mapLayoutBH.size());
    char pHttpURL[128] = { 0 };
    sprintf_s(pHttpURL, sizeof(pHttpURL), "http://%s:%d/Store/delpicture", m_pDBInfo->pBatchStoreServerIP, m_pDBInfo->nBatchStoreServerPort);
    CHttpSyncClientPtr pClient(NULL);

    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType&allocator = document.GetAllocator();

    map<string, string>::iterator it = m_mapLayoutBH.begin();
    while (it != m_mapLayoutBH.end())
    {
        rapidjson::Value array(rapidjson::kArrayType);
        int i = 0;
        while (it != m_mapLayoutBH.end())
        {
            if (i < 10)
            {
                rapidjson::Value object(rapidjson::kObjectType);
                object.AddMember(JSONFACEUUID, rapidjson::StringRef(it->second.c_str()), allocator);
                array.PushBack(object, allocator);
                i++;
                it++;
                if (i < 10 && it != m_mapLayoutBH.end())
                {
                    continue;
                }
            }

            if (i == 10 || it == m_mapLayoutBH.end())
            {
                document.AddMember(JSONSTORELIBID, DQDDCSTORELIBID, allocator);
                document.AddMember(JSONSTOREFACE, array, allocator);

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                document.Accept(writer);
                string sDelInfo = string(buffer.GetString());

                //向批量布控服务发送消息
                pClient->OpenUrl("POST", pHttpURL, NULL, 0, (const BYTE*)sDelInfo.c_str(), sDelInfo.size());
                //g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "--向批量布控服务发送删除信息...");

                //收取回应消息
                LPCBYTE pImageBuf = NULL;
                int nImageLen = 0;
                pClient->GetResponseBody(&pImageBuf, &nImageLen);
                //g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "批量布控服务回应删除信息...");

                i = 0;
                document.RemoveAllMembers();
                break;
            }
        }
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向批量布控服务发送删除信息结束!");

    //从数据库删除冗余布控数据
    char pSql[8096] = { 0 };
    int nRet = 0;
    map<string, string>::iterator itDel = m_mapLayoutBH.begin();
    int nDelete = 0;    //删除布控重点人员总数
    for (; itDel != m_mapLayoutBH.end(); itDel++)
    {
        nDelete++;
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "删除冗余布控信息, BH[%s].", itDel->first.c_str());

        //删除storefaceinfo表数据
        sprintf_s(pSql, sizeof(pSql), "delete from %s where zdrybh = '%s'", MYSQLSTOREFACEINFO, itDel->first.c_str());
        nRet = m_mysqltool.mysql_exec(_MYSQL_DELETE, pSql);
        if (nRet < 0)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Failed: %s", pSql);
            return false;
        }

        //删除layoutresult表数据
        sprintf_s(pSql, sizeof(pSql), "delete from %s where layoutfaceuuid = '%s'", MYSQLLAYOUTRESULT, itDel->second.c_str());
        nRet = m_mysqltool.mysql_exec(_MYSQL_DELETE, pSql);
        if (nRet < 0)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Failed: %s", pSql);
            return false;
        }
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "删除冗余布控人数结束, 删除数量: %d.", nDelete);
    return true;
}
//更新库人脸数量到storecount表
bool CUpdateLib::UpdateStoreCount()
{
    char pSql[1024 * 8] = { 0 };

    sprintf_s(pSql, sizeof(pSql),
        "update %s set count = "
        "(select count(*) from %s where layoutlibid = %d) "
        "where storelibid = %d", 
        MYSQLSTORECOUNT, MYSQLSTOREFACEINFO, DQDDCSTORELIBID, DQDDCSTORELIBID);
    int nRet = m_mysqltool.mysql_exec(_MYSQL_UPDATA, pSql);
    if (nRet < 0)
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Failed: %s", pSql);
        return false;
    }
    return true;
}