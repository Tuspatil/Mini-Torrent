// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <cmath>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <list>
#include <fstream>
#include <cmath>
#include <vector>
#include <iostream>
#include <errno.h>
#include <unordered_map>
#include <queue>
#define TPORT 5000
typedef unsigned long long int ulli;
using namespace std;

bool KEEP_LISTENING;
string trackerip = "127.0.0.1";
int tracker_port;
string self_ip="127.0.0.1";
string self_port;
int CPORT;
string self_gid;
string self_cid;
unsigned long long int chunksize = 524288;
//524288        512kb
//1024          1kb
struct add_request{
    string cid;
    string gid;
    string ip;
    string port;
};

unordered_map<string,string> filetohash;
unordered_map<string,string> hashtofile;
queue<add_request> request_queue;

unordered_map<string,vector<char>> master;
/*
     msg_type
    client - client ids 31 to 50 are reserved
    convention : i for message, i+1 for acknowledgment
    31s      add_me_to_group
    32t      ack_add_me_to_group         status
    33u      get_file_avail              gid filename    hash
    34v      ack_get_file_avail          gid filename    hash    array_of_cnos
    35w      get_file_chunk              gid filename    cno
    36x      ack_get_file_chunk          gid filename    cno     chunk_of_size_512
*/
/*
    msg_type
    client - tracker ids 1 to 30 are reserved
    convention : i for message I'll receive, i+1 for acknowledgment
    2 fields are always there.
    status
    error(if status is nonzero)
    id      name                        params
    1a       acc_creation                id  pass
    2b       ack_acc_creation            status
    3c       acc_login                   id  pass
    4d       ack_acc_login               status
    5e       group_list
    6f       ack_group_list              list_of_gids
    7g       get_group_owner             gid
    8h       ack_get_group_owner         gid  port
    9i       get_file_list               gid
    10j      ack_get_file_list           gid list_of_filenames
    11k      get_clients_for_file        gid filename
    12l      ack_get_clients_for_file    gid filename    hash    list_of_ports
    13m      upload_file                 gid filename    hash
    14n      ack_upload_file             status
*/

int getport(string port)
{
    int result=0;
    for(int i=0;i<port.size();i++)
    {
        result = result*10 + (port[i]-'0');
    }
    return result;
}

void acc_creation_ack(string status,string cid)
{
    cout<<status;
    if(status == "success")
    {
        self_cid = cid;
        cout<<"account Successfully created "<<endl;
    }else{
        cout<<status;
    }
}

void acc_login_ack(string status)
{
    if(status == "success")
    {
        cout<<"Logged in "<<endl;
    }else{
        cout<<status;
    }
}

void create_group_ack(string status)
{
    if(status == "success")
    {
        cout<<"Group created successfully "<<endl;
    }else{
        cout<<status;
    }
}

void group_list_ack(string status,list<string> list_of_gids)
{
    if(status == "success")
    {
        auto it = list_of_gids.begin();
        string gno;
        cout<<"List of Groups "<<endl;
        cout<<"hihi";
        while(it != list_of_gids.end())
        {
            cout<<*it<<endl;
            it++;
        }
    }else{
        cout<<status;
    }
}


void send_joining_request(string gid,string ip,string port)
{
    string message = "s:success:"+self_cid+":"+ gid+":"+self_ip+":"+self_port+":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( getport(port) );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Joining Request sent to owner"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    strcpy(status,strtok_r(rec_buffer,":",&saveptr1));
    if(status == "success")
    {
        cout<<"Request received by group owner "<<endl;
    }
    else{
        cout<<"Wrong message type received "<<endl;
    }
}
void group_owner_ack(string status,string gid, string ip,string port)
{
    char choice;
    if(status == "success")
    {
        cout<<"Received Group owner as "<<ip<<":"<<port<<endl;
        cout<<"Send request?  y/n    ";
        cin>>choice;
        if(choice == 'n')
        {
            return;
        }
        cout<<endl;
        cout<<"Sending request "<<endl;
        cout<<"gid here is "<<gid<<endl;
        send_joining_request(gid,ip,port);
    }else{
        cout<<status;
    }
}

void filenames_list_ack(string status,string gid,list<string> list_of_filenames)
{
    if(status == "success")
    {
        cout<<"\nFile list for gid "<<gid<<endl;
        auto it = list_of_filenames.begin();
        while(it != list_of_filenames.end())
        {
            cout<<*it<<endl;
            it++;
        }
    }else{
        cout<<status;
    }
}

void send_file_avail(string sha,int socket)
{
    auto it = master.find(sha);
    if(it != master.end())
    {
        unsigned long int number_of_chunks = (it->second).size();
        string tempchunks = "v:" + to_string(number_of_chunks) + ":";
        int tempbuffersize = tempchunks.size()+1;
        char tempbuffer[tempbuffersize];
        strcpy(tempbuffer,tempchunks.c_str());
        send(socket,tempbuffer,tempbuffersize,0);
        char status[2];
        recv(socket,status,2,0);
        if(status[0]=='s')
        {
            auto it1 = (it->second).begin();
            int i=0;
            int temp = (it->second).size()+1;
            char buffer[temp];
            while(it1 != (it->second).end())
            {
                buffer[i]=*it1;
                it1++;
                i++;
            }
            buffer[i]='\0';
            send(socket,buffer,temp,0);
        }else{
            cout<<"received "<<status;
        }
    }else{
        cout<<"Did not find entry in hash "<<endl;
    }
}


void get_file_avail(string sha1,char *arr,unsigned long int number_of_chunks,string ipport)
{
    string message = "u:success:"+sha1+":";
    int i=0;
    int j;
    int count=0;
    while(ipport[i]!=',')
    {
        i++;
    }
    j=i+1;
    char ip[i];
    while(i)
    {
        ip[count]=ipport[count];
        count++;
        i--;
    }
    ip[count]='\0';
    count=j;
    while(ipport[j]!='\0')
    {
        j++;
    }
    i=0;
    char port[j-count+1];
    while(count!=j)
    {
        port[i]=ipport[count];
        count++;
        i++;
    }
    port[i]='\0';
    message = "u:success:"+sha1+":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( getport(port) );

	if(inet_pton(AF_INET, trackerip.c_str(), &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Chunk Request sent to "<<ipport<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* schunk_count_at_sender = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    schunk_count_at_sender = strtok_r(rec_buffer,":",&saveptr1);
    if(msgtype == 'v')
    {
        strcpy(schunk_count_at_sender,strtok_r(NULL,":",&saveptr1));
        int l=0;
        unsigned long int chunk_count_at_sender=0;
        while(schunk_count_at_sender[l]!='\0')
        {
            chunk_count_at_sender = chunk_count_at_sender * 10 + (schunk_count_at_sender[l]-'0');
            l++;
        }
        if(chunk_count_at_sender != number_of_chunks)
        {
            cout<<"Chunk count mismatch"<<endl;
            return;
        }
        char tempbuf[2];
        tempbuf[0]='s';
        tempbuf[1]='\0';
        send(sock,tempbuf,2,0);
        char rec_buffer_for_vector[number_of_chunks+1];
        recv(sock,rec_buffer_for_vector,number_of_chunks+1,0);
        for(int i=0;i<number_of_chunks;i++)
        {
            arr[i]=rec_buffer_for_vector[i];
        }
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

void sendChunk(string status, string sha, int cno,int socket)
{
    auto it = master.find(sha);
    if(it != master.end())
    {
        if((it->second)[cno] != 1)
        {
            cout<<"Chunk not found "<<endl;
            return;
        }
        auto it1 = hashtofile.find(sha);
        if(it1 != hashtofile.end())
        {
            string path = it1->second;
            FILE *fptr;
            fptr = fopen(path.c_str(),"r");
            fseek(fptr,cno*chunksize,SEEK_SET);
            int size_of_data2bsent = chunksize;
            char buffer[size_of_data2bsent];
            int val = fread(buffer,1,chunksize,fptr);
            string message = "s:"+ to_string(val) + ":" + buffer;
            char chunk_send_buffer[message.size()+1];
            strcpy(chunk_send_buffer,message.c_str());
            send(socket,chunk_send_buffer,message.size()+1,0);
        }else{
            cout<<"hashtofile error"<<endl;
            string message = "n:";
            char buff[3];
            strcpy(buff,message.c_str());
            send(socket,buff,3,0);
        }
    }else{
        cout<<"master error"<<endl;
    }
}

void getChunk(string ipport,int chunk_no,string sha,string downloadpath)
{
    int i=0;
    int j;
    int count=0;
    while(ipport[i]!=',')
    {
        i++;
    }
    j=i+1;
    char ip[i];
    while(i)
    {
        ip[count]=ipport[count];
        count++;
        i--;
    }
    ip[count]='\0';
    count=j;
    while(ipport[j]!='\0')
    {
        j++;
    }
    i=0;
    char port[j-count+1];
    while(count!=j)
    {
        port[i]=ipport[count];
        count++;
        i++;
    }
    port[i]='\0';
    //got ip and port now establish connection
    string message = "w:"+sha+":"+to_string(chunk_no)+":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	int rec_buffer_size = chunksize+15;
	char rec_buffer[rec_buffer_size] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( getport(port) );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Chunk send request sent to owner"<<endl;
    valread = recv(sock,rec_buffer, rec_buffer_size,0);
    cout<<"Got "<<valread<<" bytes "<<endl;
    char msgtype = rec_buffer[0];
    char *saveptr1;
    char* size_of_chunk = new char[20];
    strcpy(size_of_chunk,strtok_r(NULL,":",&saveptr1));
    if(msgtype == 's')
    {
        strcpy(size_of_chunk,strtok_r(NULL,":",&saveptr1));
        unsigned long long int chunk_size = 0;
        int i=0;
        while(size_of_chunk[i]!='\0')
        {
            chunk_size = chunk_size*10 + (size_of_chunk[i]-'0');
            i++;
        }
        i=0;
        while(rec_buffer[i++]!=':');
        while(rec_buffer[i++]!=':');
        char chunkbuffer[chunk_size];
        strcpy(chunkbuffer,rec_buffer+i);
        fstream f;
        f.open(downloadpath,ios::in| ios::out| ios::binary);
        f.seekg(chunk_no*chunk_size,f.beg);
        f.write(chunkbuffer,chunk_size);
        //write this chunk to file
    }
    else{
        if(msgtype == 'n')
        {
            cout<<"Client sent n. Chunk not present (maybe)"<<endl;
        }else{
            cout<<"Wrong message type received "<<endl;
        }
    }
}


void clients_list_for_file(string status,string gid,string sha1,string sfilesize, list<string> ports,string downloadpath)
{
    unsigned long long int filesize=0;
    int i=0;
    filetohash[downloadpath]=sha1;
    hashtofile[sha1]=downloadpath;
    unordered_map<int,string> indextoipport;
    while(sfilesize[i]!= '\0')
    {
        filesize = filesize*10 + (sfilesize[i]-'0');
        i++;
    }
    i=0;
    unsigned long int number_of_chunks = ceil((double)filesize/chunksize);
    vector<char> temp(number_of_chunks,0);
    master[sha1]=temp;
    auto it = ports.begin();
    int index=0;
    char **arr;
    arr = new char*[ports.size()];
    for(int k=0;k<ports.size();k++)
    {
        arr[k] = new char[number_of_chunks];
    }
    while(it != ports.end())
    {
        indextoipport[index]=*it;
        get_file_avail(sha1,arr[index],number_of_chunks,*it);
        //array filled for that ip port
        it++;
    }
    //algo
    bool flag=true;
    unsigned int j=0;
    bool isincomplete = false;
    auto it2 = master.find(sha1);
    for(int i=0;isincomplete;(i=(i+1)%number_of_chunks))
    {
        flag=true;
        j=0;
        while(flag)
        {
            if((arr[i][j]==1) && (it2->second)[j] == 0)
            {
                auto it = indextoipport.find(i);
                if(it != indextoipport.end())
                {
                    string ipport = it->second;
                    getChunk(ipport,j,sha1,downloadpath);
                    (it2->second)[j]=1;
                }
                flag=false;
            }
            j=(j+1)%ports.size();
        }
        isincomplete=false;
        for(int p=0;p<(it2->second).size();p++)
        {
            if((it2->second)[p] == 0)
            {
                isincomplete=true;
            }
        }
    }
}

void upload_file_ack(string status)
{
    if(status == "success")
    {
        cout<<"File upload successful"<<endl;
    }else{
        cout<<status;
    }
}

void create_account(string id,string password)
{
    string message="a";
    message = message + ":success" + ":" + id + ":" + password + ":" + self_ip + ":" + self_port +":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( TPORT );

	if(inet_pton(AF_INET, trackerip.c_str(), &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Login Request sent"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    status = strtok_r(rec_buffer,":",&saveptr1);
    char cid[5];
    if(msgtype == 'b')
    {
        strcpy(status,strtok_r(NULL,":",&saveptr1));
        strcpy(cid,strtok_r(NULL,":",&saveptr1));
        cout<<"cid i got is "<<cid<<endl;
        self_cid = cid;
        acc_creation_ack(status,cid);
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

void login_account(string id,string password)
{
    string message="c";
    message = message + ":success" + ":" + id + ":" + password + ":" + self_ip + ":" + self_port +":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( TPORT );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Login Request sent"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    status = strtok_r(rec_buffer,":",&saveptr1);
    if(msgtype == 'd')
    {
        strcpy(status,strtok_r(NULL,":",&saveptr1));
        acc_login_ack(status);
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

void create_group(string groupid)
{
    string message="o:success:"+self_cid+":"+groupid+":"+self_ip+":"+self_port+":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( TPORT );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Create group Request sent"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    status = strtok_r(rec_buffer,":",&saveptr1);
    if(msgtype == 'p')
    {
        strcpy(status,strtok_r(NULL,":",&saveptr1));
        create_group_ack(status);
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

void join_group(string groupid)
{
    string message = "g:success:"+groupid+":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( TPORT );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"group owner details request sent"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char gid[5];
    char ip[20];
    char port[5];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    status = strtok_r(rec_buffer,":",&saveptr1);
    if(msgtype == 'h')
    {
        strcpy(status,strtok_r(NULL,":",&saveptr1));
        strcpy(gid,strtok_r(NULL,":",&saveptr1));
        strcpy(ip,strtok_r(NULL,":",&saveptr1));
        strcpy(port,strtok_r(NULL,":",&saveptr1));
        group_owner_ack(status,gid,ip,port);
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

void add_me_to_group(string status, string cid,string gid,string ip,string port,int socket)
{
    struct add_request request;
    request.gid = gid;
    request.cid = cid;
    request.ip = ip;
    request.port = port;
    request_queue.push(request);
    cout<<"Got request to join group "<<gid<<" from client "<<cid<<endl;
    string reply = "success:";
    char buffer[reply.size()+1];
	strcpy(buffer,reply.c_str());
	cout<<buffer<<endl;
	send(socket,buffer,strlen(buffer),0);
}

void leave_group(string groupid)
{
    string message = "q:success:"+groupid+":"+self_cid+":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( TPORT );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Leave group Request sent"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    status = strtok_r(rec_buffer,":",&saveptr1);
    if(status == "success")
    {
        cout<<"Left group successfully"<<endl;
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

void list_requests(string groupid)
{
    struct add_request tempreq;
    cout<<endl;
    for(int i=0;i<request_queue.size();i++)
    {
        tempreq = request_queue.front();
        request_queue.pop();
        if(tempreq.gid == groupid)
        {
            cout<<tempreq.cid<<endl;
        }
        request_queue.push(tempreq);
    }
}

void accept_request(string groupid,string cid)
{
    struct add_request tempreq;
    cout<<endl;
    for(int i=0;i<request_queue.size();i++)
    {
        tempreq = request_queue.front();
        request_queue.pop();
        if((tempreq.gid == groupid) && (tempreq.cid == cid))
        {
            cout<<"Sending accept signal to "<<tempreq.cid<<endl;
            string message = "t:success:"+tempreq.gid+":";
            char buffer[message.size()+1];
            strcpy(buffer,message.c_str());
            int sock = 0, valread;
            struct sockaddr_in serv_addr;
            char rec_buffer[1024] = {0};
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                printf("\n Socket creation error \n");
                return;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons( getport(tempreq.port) );

            if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
            {
                printf("\nInvalid address/ Address not supported \n");
                return;
            }

            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            {
                cout<<strerror(errno)<<endl;
                printf("\nConnection Failed \n");
                return;
            }
            send(sock , buffer , strlen(buffer) , 0 );
            //client infromed
            close(sock);

            //informing tracker to update the group_membership map

            message = "0:success:"+tempreq.gid+":"+tempreq.cid+":";
            char newbuffer[message.size()+1];
            strcpy(newbuffer,message.c_str());
            sock = 0;
            struct sockaddr_in serv_addr1;
            char rec_buffer1[1024] = {0};
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                printf("\n Socket creation error \n");
                return;
            }

            serv_addr1.sin_family = AF_INET;
            serv_addr1.sin_port = htons( TPORT );

            if(inet_pton(AF_INET, "127.0.0.1", &serv_addr1.sin_addr)<=0)
            {
                printf("\nInvalid address/ Address not supported \n");
                return;
            }

            if (connect(sock, (struct sockaddr *)&serv_addr1, sizeof(serv_addr1)) < 0)
            {
                cout<<strerror(errno)<<endl;
                printf("\nConnection Failed \n");
                return;
            }
            send(sock , newbuffer , strlen(newbuffer) , 0 );
            cout<<"Informing tracker "<<endl;
            valread = recv(sock,rec_buffer1, 1024,0);
            cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer1<<endl;
            char* status = new char[20];
            char msgtype = rec_buffer1[0];
            char *saveptr1;
            strcpy(status,strtok_r(rec_buffer1,":",&saveptr1));
            if(status == "success")
            {
                cout<<"Tracker updated successfully "<<endl;
            }
            else{
                cout<<"Wrong message type received "<<endl;
            }
            return;
        }
        request_queue.push(tempreq);
    }
}

void request_acceptance(string status,string gid)
{
    cout<<"Request to joing group "<<gid<<" has been approved "<<endl;
}

void list_groups()
{
    string message="e:success:";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( TPORT );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Group ids Request sent"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    status = strtok_r(rec_buffer,":",&saveptr1);
    list<string> list_of_gids;
    if(msgtype == 'f')
    {
        strcpy(status,strtok_r(NULL,":",&saveptr1));
        char* gid_tokens = new char[5];
        strcpy(gid_tokens,strtok_r(NULL,":",&saveptr1));
        while(gid_tokens)
        {
            cout<<"hi"<<gid_tokens<<endl;
            list_of_gids.push_back(gid_tokens);
            gid_tokens=strtok_r(NULL,":",&saveptr1);
        }
        group_list_ack(status,list_of_gids);
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

void list_files(string groupid)
{
    string message="i:success:"+groupid+":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( TPORT );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Files list Request sent"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    char gid[5];
    char filename[50];
    list<string> list_of_filenames;
    status = strtok_r(rec_buffer,":",&saveptr1);
    list<string> list_of_gids;
    if(msgtype == 'j')
    {
        strcpy(status,strtok_r(NULL,":",&saveptr1));
        strcpy(gid,strtok_r(NULL,":",&saveptr1));
        strcpy(filename,strtok_r(NULL,":",&saveptr1));
        while(filename)
        {
            list_of_filenames.push_back(filename);
            strcpy(filename,strtok_r(NULL,":",&saveptr1));
        }
        filenames_list_ack(status,gid,list_of_filenames);
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

string getFileName(string path)
{
    int i=0,count=0,j,temp;
    while(path[i++] != '\0');
    i--;
    while(path[i--] != '/')
        count++;
    i+=2;
    j=0;
    string name = "";
    while(count--)
        name = name + path[i++];
    return name;
}

unsigned long long int getSize(string path)
{
    ifstream ifs(path);
    ifs.seekg(0, ios::end);
    unsigned long long int sizeInBytes = ifs.tellg();
    ifs.close();
    return sizeInBytes;
}

string getSha(string path)
{

    char* file_buffer;
    unsigned char *result = new unsigned char[20];
    int file_descript = open(path.c_str(), O_RDONLY);
    unsigned long long int sizeInBytes = getSize(path);
    file_buffer = (char*)mmap(0, sizeInBytes, PROT_READ, MAP_SHARED, file_descript, 0);
    SHA1((unsigned char*) file_buffer, sizeInBytes, result);
    munmap(file_buffer, sizeInBytes);
    close(file_descript);
    string tempo(reinterpret_cast<char*>(result));
    return tempo;
}

void print_sha1(unsigned char* result) {
    int i;
    for(i=0; i <20; i++) {
            printf("%02x",result[i]);
    }
    cout<<endl;
}


void upload_file(string path,string groupid)
{
    string filename = getFileName(path);
    string sha = getSha(path);
    unsigned long int filesize = getSize(path);
    unsigned long int number_of_chunks = ceil((double)filesize/chunksize);
    vector<char> temp(number_of_chunks,1);
    master[sha]=temp;
    filetohash[path]=sha;
    hashtofile[sha]=path;
    string message = "m:success:"+self_cid+":"+groupid+":"+filename+":"+sha+":"+to_string(getSize(path))+":";
    cout<<"message is "<<message<<endl;
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( TPORT );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Upload Request sent"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    status = strtok_r(rec_buffer,":",&saveptr1);
    list<string> list_of_gids;
    if(msgtype == 'n')
    {
        strcpy(status,strtok_r(NULL,":",&saveptr1));
        if(status == "success")
        {
            cout<<"Successfully uploaded "<<endl;
        }else{
            cout<<status;
        }
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

void download_file(string groupid,string filename,string downloadpath)
{
    string message = "k:success:"+self_cid+":"+groupid+":"+filename+":";
    char buffer[message.size()+1];
    strcpy(buffer,message.c_str());
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char rec_buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( TPORT );

	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
        cout<<strerror(errno)<<endl;
		printf("\nConnection Failed \n");
		return;
	}
	send(sock , buffer , strlen(buffer) , 0 );
    cout<<"Download data Request sent"<<endl;
    valread = recv(sock,rec_buffer, 1024,0);
    cout<<"Got "<<valread<<" bytes and message is "<<rec_buffer<<endl;
    char* status = new char[20];
    char msgtype = rec_buffer[0];
    char *saveptr1;
    char gid[5];
    char sha1[20];
    char filesize[20];
    char port[5];
    list<string> ports;
    status = strtok_r(rec_buffer,":",&saveptr1);
    list<string> list_of_gids;
    if(msgtype == 'l')
    {
        strcpy(status,strtok_r(NULL,":",&saveptr1));
        strcpy(gid,strtok_r(NULL,":",&saveptr1));
        strcpy(sha1,strtok_r(NULL,":",&saveptr1));
        strcpy(filesize,strtok_r(NULL,":",&saveptr1));
        strcpy(port,strtok_r(NULL,":",&saveptr1));
        while(port)
        {
            ports.push_back(port);
            strcpy(port,strtok_r(NULL,":",&saveptr1));
        }
        clients_list_for_file(status,gid,sha1,filesize,ports,downloadpath);
    }else{
        cout<<"Wrong message type received "<<endl;
    }
}

void logout()
{

}

void show_downloads()
{

}

void stop_share(string groupid,string filename)
{

}

void *parseInput(void *vargp)
{
    int valread;
    char buffer[1024] = {0};
    int *temp = (int *)vargp;
    int socket = *temp;
    cout<<"in thread "<<getpid()<<endl;
    valread = recv( socket , buffer, 1024,0);
    printf("%d\n",valread );
	cout<<"yolo";
	cout<<"received buffer \n"<<buffer<<endl;
    char* saveptr1,*saveptr2,*saveptr3;
    char msg_type;
    char* cid = new char[5];
    char* password = new char[20];
    char* gid = new char[5];
    char* filename = new char[50];
    char* filesize = new char[20];
    char* sha1 = new char[20];
    char* port = new char[5];
    char* ip = new char[20];
    list<string> list_of_gids;
    list<string> list_of_filenames;
    list<string> ports;
    char* status = new char[20];
    msg_type = buffer[0];
    char *token = strtok_r(buffer,":",&saveptr1);
    switch(msg_type)
    {
    	case 'b':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(cid,strtok_r(NULL,":",&saveptr1));
    		acc_creation_ack(status,cid);
    		break;
    	}
    	case 'd':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		acc_login_ack(status);
    		break;
    	}
    	case 'f':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		char* gid_tokens = new char[5];
    		strcpy(gid_tokens,strtok_r(NULL,":",&saveptr1));
    		while(gid_tokens)
    		{
    			list_of_gids.push_back(gid_tokens);
    			strcpy(gid_tokens,strtok_r(NULL,":",&saveptr1));
    		}
    		group_list_ack(status,list_of_gids);
    		break;
    	}
    	case 'h':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		strcpy(ip,strtok_r(NULL,":",&saveptr1));
    		strcpy(port,strtok_r(NULL,":",&saveptr1));
    		group_owner_ack(status,gid,ip,port);
    		break;
    	}
    	case 'j':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		strcpy(filename,strtok_r(NULL,":",&saveptr1));
    		while(filename)
    		{
    			list_of_filenames.push_back(filename);
    			strcpy(filename,strtok_r(NULL,":",&saveptr1));
    		}
    		filenames_list_ack(status,gid,list_of_filenames);
    		break;
    	}
    	case 'l':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		strcpy(sha1,strtok_r(NULL,":",&saveptr1));
    		strcpy(port,strtok_r(NULL,":",&saveptr1));
    		while(port)
    		{
    			ports.push_back(port);
    			strcpy(port,strtok_r(NULL,":",&saveptr1));
    		}
    		//clients_list_for_file(status,gid,sha1,ports);
    		break;
    	}
    	case 'n':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		upload_file_ack(status);
    		break;
    	}
    	case 't':       //required
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		request_acceptance(status,gid);
    		break;
    	}
    	case 'v':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		strcpy(filename,strtok_r(NULL,":",&saveptr1));
    		strcpy(sha1,strtok_r(NULL,":",&saveptr1));
			vector<int> chunk;
			char* temp;
			strcpy(temp,strtok_r(NULL,":",&saveptr1));
			int cno,i;
			while(temp)
			{
				i=0;
				cno=0;
				while(temp[i]!='\0')
				{
					cno = cno * 10 + (temp[i]-'0');
					i++;
				}
				chunk.push_back(cno);
			}
			//file_avail_ack(status,gid,filename,sha1,chunk);
    		break;
    	}
    	case 'x':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		strcpy(sha1,strtok_r(NULL,":",&saveptr1));
    		int cno=0;
    		char* temp = new char[20];
    		strtok_r(NULL,":",&saveptr1);
    		int i=0;
    		while(temp[i]!='\0')
    		{
    			cno = cno * 10 + (temp[i]-'0');
    			i++;
    		}
    		strcpy(temp,strtok_r(NULL,":",&saveptr1));
    		ulli size=0;
    		i=0;
    		while(temp[i]!='\0')
    		{
    			size = size * 10 + (temp[i]-'0');
    			i++;
    		}
    		//file_chunk_info(status,gid,sha1,cno,size);
    		break;
    	}
    	case 's':         //required
    	{
            strcpy(status,strtok_r(NULL,":",&saveptr1));
            strcpy(cid,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		strcpy(ip,strtok_r(NULL,":",&saveptr1));
    		strcpy(port,strtok_r(NULL,":",&saveptr1));
    		add_me_to_group(status,cid,gid,ip,port,socket);
    		break;
    	}
    	case 'u':
    	{
            strcpy(status,strtok_r(NULL,":",&saveptr1));
            strcpy(sha1,strtok_r(NULL,":",&saveptr1));
            send_file_avail(sha1,socket);
            break;
    	}
    	case 'w':
    	{
            strcpy(status,strtok_r(NULL,":",&saveptr1));
            strcpy(sha1,strtok_r(NULL,":",&saveptr1));
            char chunkNumber[20];
            strcpy(chunkNumber,strtok_r(NULL,":",&saveptr1));
            unsigned long int cno = 0;
            int i=0;
            while(chunkNumber[i] != '\0')
            {
                cno = cno*10 + (chunkNumber[i]-'0');
                i++;
            }
            sendChunk(status,sha1,cno,socket);
            break;
    	}
    }
    return NULL;
}

/*
void sendMessage(string message,string ip,string port)
{
    int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char hello[] = "Hello from client";
	char buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return NULL;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return NULL;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return NULL;
	}
	send(sock , hello , strlen(hello) , 0 );
	printf("Hello message sent\n");
	valread = read( sock , buffer, 1024);
	printf("%s\n",buffer );
}
*/

void* listenerThread(void* arg)
{
    int server_fd;
    while(KEEP_LISTENING)
    {
        int new_socket, valread;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        char buffer[1024] = {0};
        char hello[] = "Hello from server";
        pthread_t thread_id;
        // Creating socket file descriptor
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        // Forcefully attaching socket to the port 8080
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                    &opt, sizeof(opt)))
        {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons( CPORT );

        // Forcefully attaching socket to the port 8080
        if (bind(server_fd, (struct sockaddr *)&address,
                                    sizeof(address))<0)
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }
        if (listen(server_fd, 3) < 0)
        {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        while(1)
        {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                        (socklen_t*)&addrlen))<0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        pthread_create(&thread_id, NULL, parseInput, (void *)&new_socket);
        }
    }
    close(server_fd);

}

int main(int argc, char const *argv[])
{
	int portno=0;
    self_port = argv[1];
    CPORT = getport(self_port);
    KEEP_LISTENING = true;
    pthread_t listenerThreadid;
    pthread_create(&listenerThreadid, NULL, listenerThread,NULL);
    string id,password,groupid,userid,uploadpath,filename,downloadpath;
    //thread seperated
    int choice;
    do
    {
        cin>>choice;
        switch(choice)
        {
            case 1:
            {
                //create acc        id  pass
                cin>>id;
                cin>>password;
                create_account(id,password);
                break;
            }
            case 2:
            {
                //login             id  pass
                cin>>id;
                cin>>password;
                login_account(id,password);
                break;
            }
            case 3:
            {
                //creategroup       groupid
                cin>>groupid;
                create_group(groupid);
                break;
            }
            case 4:
            {
                //joingroup         groupid
                cin>>groupid;
                join_group(groupid);
                break;
            }
            case 5:
            {
                //leavegroup        groupid
                cin>>groupid;
                leave_group(groupid);
                break;
            }
            case 6:
            {
                //listrequests      groupid
                cin>>groupid;
                list_requests(groupid);
                break;
            }
            case 7:
            {
                //acceptrequests    groupid userid
                cin>>groupid;
                cin>>userid;
                accept_request(groupid,userid);
                break;
            }
            case 8:
            {
                //listgroups
                list_groups();
                break;
            }
            case 9:
            {
                //listfiles         groupid
                cin>>groupid;
                list_files(groupid);
                break;
            }
            case 10:
            {
                //uploadfile        path    groupid
                cin>>uploadpath;
                cin>>groupid;
                upload_file(uploadpath,groupid);
                break;
            }
            case 11:
            {
                //downloadfile      groupid filename    destinationpath
                cin>>groupid;
                cin>>filename;
                cin>>downloadpath;
                download_file(groupid,filename,downloadpath);
                break;
            }
            case 12:
            {
                //logout
                logout();
                break;
            }
            case 13:
            {
                //showD
                show_downloads();
                break;
            }
            case 14:
            {
                //stopshare     groupid filename
                cin>>groupid;
                cin>>filename;
                stop_share(groupid,filename);
                break;
            }
            case 15:
            {
                //stop server
                KEEP_LISTENING=false;
                cout<<"Stopped"<<endl;
                break;
            }
            default:
            {
                cout<<"Invalid choice"<<endl;
                break;
            }
        }
    }while(choice != 15);
    return 0;
}

