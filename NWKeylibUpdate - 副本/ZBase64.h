#include <string>
using namespace std;

#ifndef _ZBASE64_
#define _ZBASE64_
class ZBase64
{
public:
    /*����
    DataByte
        [in]��������ݳ���,���ֽ�Ϊ��λ
    */
    static string Encode(const unsigned char* Data,int DataByte);
    /*����
    DataByte
        [in]��������ݳ���,���ֽ�Ϊ��λ
    OutByte
        [out]��������ݳ���,���ֽ�Ϊ��λ,�벻Ҫͨ������ֵ����
        ������ݵĳ���
    */
    static string Decode(const char* Data,int DataByte,int& OutByte);
};
#endif