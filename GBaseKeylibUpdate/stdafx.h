// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#define WIN32_LEAN_AND_MEAN		// �� Windows ͷ���ų�����ʹ�õ�����

#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include "LogRecorder.h"
#pragma comment(lib, "HPSocket.lib")

// TODO:  �ڴ˴����ó�����Ҫ������ͷ�ļ�

#define MAXLEN      128
#define MAXIPLEN        20          //IP, FaceRect��󳤶�
#define FEATURELEN  4096
#define FEATUREMIXLEN   500         //Feature��̳���, С�ڴ˳��ȶ�Ϊ���Ϸ�
#define THREADNUM  8           //��ȡ����ֵ�����߳���
#define THREADWAITTIME 5


