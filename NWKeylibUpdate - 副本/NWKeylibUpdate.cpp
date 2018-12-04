// NWKeylibUpdate.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "UpdateLib.h"

#ifdef _DEBUG
int main()
{
    CUpdateLib UpdateLib;
    if (!UpdateLib.StartUpdate())
    {
        printf("Update Lib Failed!\n");
    }
    else
    {
        printf("Update Lib Success!\n");
    }
    getchar();
    return 0;
}


#else
#include "ntservice.h"
#include <iostream>
#define SERVICE_NAME "XSUpdateNWKeyLibServer"
//���غ�������
static BOOL WINAPI		HandlerRoutine(DWORD dwCtrlType);
static LONG __stdcall	GlobalCrashHandlerExceptionFilter(EXCEPTION_POINTERS* pExPtrs);

//����ֱ�
static CNTService NTService(SERVICE_NAME);
using namespace std;

int main(int argc, TCHAR* argv[], TCHAR* envp[])
//int main(int argc, char* argv[])
{
    int iCode = 0;

    // ��ʾ������Ϣ
    cout << (LPCTSTR)"****** ��ʼ��װ ******" << endl;

    ///////////////////////////////////
    LPTOP_LEVEL_EXCEPTION_FILTER oldExceptionFilter; ///�ɵ��쳣������
    oldExceptionFilter = SetUnhandledExceptionFilter(GlobalCrashHandlerExceptionFilter);
    ///���ÿ��ƴ�����
    SetConsoleCtrlHandler(
        (PHANDLER_ROUTINE)HandlerRoutine,	// handler function
        true								// add or remove handler
    );
    //////////////////////////////////////
    try
    {
        // Create the service object

        // ������׼�������в��� (�簲װ��ж�ء��汾��)
        NTService.ParseStandardArgs(argc, argv);
        // When we get here, the service has been stopped
        iCode = NTService.m_Status.dwWin32ExitCode;
    }
    catch (...)
    {
        OutputDebugString("ִ�з����������쳣����! \n");
        NTService.OnStop(); //ֹͣ����
    }

    //ɾ�����ƴ�����
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)HandlerRoutine,	// handler function
        false								// add or remove handler
    );
    SetUnhandledExceptionFilter(oldExceptionFilter);

    cout << (LPCTSTR)"****** ��װ��� ******" << endl;

    return iCode;

}

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:		//  0
        break;
    case CTRL_BREAK_EVENT:	//  1
        break;
    case CTRL_CLOSE_EVENT:	//  2
        break;
    case CTRL_SHUTDOWN_EVENT: //	6  
        break;
    default:
        return false;
    }

    NTService.OnStop();		//ֹͣ

    ExitProcess(0);			//ֹͣ���������

    return true;
}

LONG __stdcall GlobalCrashHandlerExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    LONG lRet = EXCEPTION_CONTINUE_SEARCH;
    __try
    {
        OutputDebugString("error on global error???");
        ; //
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        lRet = EXCEPTION_CONTINUE_SEARCH;
    }
    return (lRet);
}




#endif