#include <stdio.h>
#include <time.h>

#include <Winsock2.h>
#include <Windows.h> 

#pragma comment (lib, "ws2_32.lib")

// 2�ֽ� ���� sizeof(icmp_header) == 8 
// ����ping ��wiresharkץ���е����ݽṹ 
typedef struct icmp_header
{
    unsigned char icmp_type;    // ��Ϣ����
    unsigned char icmp_code;    // ����
    unsigned short icmp_checksum;    // У���
    unsigned short icmp_id;     // ����Ωһ��ʶ�������ID�ţ�ͨ������Ϊ����ID
    unsigned short icmp_sequence;   // ���к�
} icmp_header;

#define ICMP_HEADER_SIZE sizeof(icmp_header)
#define ICMP_ECHO_REQUEST 0x08
#define ICMP_ECHO_REPLY 0x00

// ����У��� 
unsigned short chsum(struct icmp_header *picmp, int len)
{
    long sum = 0;
    unsigned short *pusicmp = (unsigned short *)picmp;

    while ( len > 1 )
    {
        sum += *(pusicmp++);
        if ( sum & 0x80000000 )
        {
            sum = (sum & 0xffff) + (sum >> 16);
        }
        len -= 2;
    }

    if ( len )
    {
        sum += (unsigned short)*(unsigned char *)pusicmp;
    }

    while ( sum >> 16 )
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return (unsigned short)~sum;
}

static int respNum = 0;
static int minTime = 0,maxTime = 0,sumTime = 0;
int ping(char *szDestIp)
{
	//printf("destIp = %s\n",szDestIp);
    int bRet = 1;
    
    WSADATA wsaData;
    int nTimeOut = 1000;//1s  
    char szBuff[ICMP_HEADER_SIZE + 32] = { 0 };
    icmp_header *pIcmp = (icmp_header *)szBuff;
    char icmp_data[32] = { 0 };

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    // ����ԭʼ�׽���
    SOCKET s = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
    
    // ���ý��ճ�ʱ
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char const*)&nTimeOut, sizeof(nTimeOut));

    // ����Ŀ�ĵ�ַ
    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.S_un.S_addr = inet_addr(szDestIp);
    dest_addr.sin_port = htons(0);

    // ����ICMP���
    pIcmp->icmp_type = ICMP_ECHO_REQUEST;
    pIcmp->icmp_code = 0;
    pIcmp->icmp_id = (USHORT)::GetCurrentProcessId();
    pIcmp->icmp_sequence = 0;
    pIcmp->icmp_checksum = 0;

    // ������ݣ��������� 
    memcpy((szBuff + ICMP_HEADER_SIZE), "abcdefghijklmnopqrstuvwabcdefghi", 32);
    
    // ����У���
    pIcmp->icmp_checksum = chsum((struct icmp_header *)szBuff, sizeof(szBuff));

    sockaddr_in from_addr;
    char szRecvBuff[1024];
    int nLen = sizeof(from_addr);
    int ret,flag = 0; 

	DWORD  start = GetTickCount();
    ret = sendto(s, szBuff, sizeof(szBuff), 0, (SOCKADDR *)&dest_addr, sizeof(SOCKADDR));
    //printf("ret = %d ,errorCode:%d\n",ret ,WSAGetLastError() ); 
   
	int i = 0; 
	//����һ��Ҫ��whileѭ������Ϊrecvfrom ����ܵ��ܶ౨�ģ����� ���ͳ�ȥ�ı���Ҳ�ᱻ�յ��� ����������� wireshark ץ���鿴����������������һ���� �Ų�������� 
    while(1){
    	if(i++ > 5){// icmp���� ���������Ŀ���������ǲ��᷵�ر��ģ��ೢ�Լ��ν������ݣ������û�յ� ������ʧ�� 
    		flag = 1;
    		break;
		}
    	memset(szRecvBuff,0,1024);
    	//printf("errorCode1:%d\n",WSAGetLastError() ); 
    	int ret = recvfrom(s, szRecvBuff, MAXBYTE, 0, (SOCKADDR *)&from_addr, &nLen);
    	//printf("errorCode2:%d\n",WSAGetLastError() ); 
    	//printf("ret=%d,%s\n",ret,inet_ntoa(from_addr.sin_addr)) ; 
    	//���ܵ� Ŀ��ip�� ���� 
    	if( strcmp(inet_ntoa(from_addr.sin_addr),szDestIp) == 0)  {
    		respNum++;
    		break;
		}
	} 
	
    
    	
	DWORD  end = GetTickCount();
	DWORD time = end -start; 
	
	if(flag){
		printf("����ʱ��\n");
		return bRet;
	}
	sumTime += time;
	if( minTime > time){
		minTime = time;
	}
	if( maxTime < time){
		maxTime = time;
	}
	
	
	
	// Windows��ԭʼ�׽��� ������ϵͳû��ȥ��IPЭ��ͷ����Ҫ�����Լ�����
	// ipͷ���ĵ�һ���ֽڣ�ֻ��1���ֽڲ��漰��С�����⣩��ǰ4λ ��ʾ ipЭ��汾�ţ���4λ ��ʾIP ͷ������(��λΪ4�ֽ�)
	char ipInfo = szRecvBuff[0];
	// ipv4ͷ���ĵ�9���ֽ�ΪTTL��ֵ
	char ttl = szRecvBuff[8];
	//printf("ipInfo = %x\n",ipInfo);
	
	
	int ipVer = ipInfo >> 4;
	int ipHeadLen = ((char)( ipInfo << 4) >> 4) * 4;
	if( ipVer  == 4) {
		//ipv4 
		//printf("ipv4 len = %d\n",ipHeadLen);
		// ���ipЭ��ͷ���õ�ICMPЭ��ͷ��λ�ã������������ֽ��� 
		// �����ֽ��� �Ǵ��ģʽ �͵�ַ ��λ�ֽ� �ߵ�ַ ��λ�ֽڡ�-> ת��Ϊ �����ֽ��� С��ģʽ �ߵ�ַ���ֽ� �͵�ַ���ֽ�   
		icmp_header* icmp_rep = (icmp_header*)(szRecvBuff + ipHeadLen);
		//����У����� 2���ֽ� �漰��С�����⣬��Ҫת���ֽ��� 
		unsigned short checksum_host = ntohs(icmp_rep->icmp_checksum);// ת�����ֽ��� ��wireshark ץȡ�ı������Ƚ� 
			
		//printf("type = %d ��checksum_host = %x\n",icmp_rep,checksum_host);

		if(icmp_rep->icmp_type == 0){ //����Ӧ���� 
			//���� 61.135.169.121 �Ļظ�: �ֽ�=32 ʱ��=1ms TTL=57
			printf("���� %s �Ļظ����ֽ�=32 ʱ��=%2dms TTL=%d checksum=0x%x \n", szDestIp, time, ttl, checksum_host);
		} else{
			bRet = 0;
			printf("����ʱ��type = %d\n",icmp_rep->icmp_type);
		}	
	}else{
		// ipv6 icmpv6 �� icmpv4 ��һ����Ҫ����Ӧ�Ĵ��� 
		//printf("ipv6 len = %d\n",ipLen); 
	} 
	
    return bRet;
}

int main(int argc, char **argv)
{
	if(argc < 2){
		printf("please input:myping ipaddr!\n");
		return 0;
	}
	printf("\n���� Ping %s ���� 32 �ֽڵ�����:\n", argv[1]);	
    int i = 0;
    
    while ( i < 4 )
    {
        int result = ping(argv[1]);
        Sleep(500);
        i ++;
    }
	
	printf("\n%s �� Ping ͳ����Ϣ:\n",argv[1]);
	printf("    ���ݰ�: �ѷ��� = %d���ѽ��� = %d����ʧ = %d (%d%% ��ʧ)��\n", i, respNum, i-respNum, (i-respNum)*100/i);
	if( i-respNum >= 4){
		return 0;
	}
	printf("�����г̵Ĺ���ʱ��(�Ժ���Ϊ��λ):\n"); 
	printf("    ��� = %dms��� = %dms��ƽ�� = %dms\n", minTime, maxTime, sumTime/respNum); 
    return 0;
}
