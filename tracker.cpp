// Server side C/C++ program to demonstrate Socket programming
#include <unistd.h>
#include <stdio.h>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <openssl/sha.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>  
#include <pthread.h>
#include <string>
#include <unordered_map>
#include <arpa/inet.h>
#include <list>

#define TPORT 5000
typedef unsigned long long int ulli;
using namespace std;
bool KEEP_LISTENING;

/*
     msg_type
    client - client ids 31 to 50 are reserved
    convention : i for message, i+1 for acknowledgment
    31      add_me_to_group
    32      ack_add_me_to_group         status
    33      get_file_avail              gid filename    hash
    34      ack_get_file_avail          gid filename    hash    array_of_cnos
    35      get_file_chunk              gid filename    cno
    36      ack_get_file_chunk          gid filename    cno     chunk_of_size_512
*/


//		cliend id 	ip,port
unordered_map<string, pair<string,string>> user_details;
//		cliend id 	password
unordered_map<string,string> authentication;
//		filename 	hash,  gid,size
unordered_map<string,pair<string,pair<string,string>>> hashvalue;
//		hash 		list of cliend ids
unordered_map<string,list<string>> client_list;
// 		groupid 	ip,port
unordered_map<string, pair<string,string>> owner_of_group;

//		gid 	list of files
unordered_map<string,list<string>> files_in_group;

//      gid     list of clients
unordered_map<string,list<string>> group_membership;


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
    ifstream ifs;
    ifs.open(path);
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


void create_account(string status,string cid,string password,string ip, string port,int socket)
{
	string reply="b";
	user_details[cid] = make_pair(ip,port);
	authentication[cid] = password;
	cout<<"acc created "<<endl;
	reply = reply + ":" + "success:"+cid+":";
	char buffer[reply.size()+1];
	strcpy(buffer,reply.c_str());
	send(socket,buffer,strlen(buffer),0);
}

void login_account(string status,string cid,string password,string ip, string port,int socket)
{
    string reply="d";
	cout<<"Logged in "<<endl;
	reply = reply + ":" + "success:";
	char buffer[reply.size()+1];
	strcpy(buffer,reply.c_str());
	send(socket,buffer,strlen(buffer),0);
	//send ack
}

void send_group_list(string status,int socket)
{
	list<string> group_list;
	string reply="f:success:";
	for(auto i : owner_of_group)
	{
		reply=reply + i.first + ":";
	}
	char buffer[reply.size()+1];
	strcpy(buffer,reply.c_str());
	cout<<buffer<<endl;
	send(socket,buffer,strlen(buffer),0);
}

void send_group_owner(string status,string gid,int socket)
{
	string reply="h:success:"+gid+":";
	auto it = owner_of_group.find(gid);
	pair<string,string> p;
	if(it != owner_of_group.end())
	{
		p = it->second;
		reply=reply + p.first + ":" + p.second + ":";
	}
	char buffer[reply.size()+1];
	strcpy(buffer,reply.c_str());
	cout<<buffer<<endl;
	send(socket,buffer,strlen(buffer),0);
}

void send_file_list(string status,string gid,int socket)
{
	string reply="j:success:"+gid+":";
	auto it1 = files_in_group.find(gid);
	list<string> temp;
	if(it1 != files_in_group.end())
	{
		temp = it1->second;
		auto it2 = temp.begin();
		while(it2 != temp.end())
		{
			reply = reply + *it2 + ":";
			it2++;
		}
	}else{
        cout<<"Group not found"<<endl;
	}
	char buffer[reply.size()+1];
	strcpy(buffer,reply.c_str());
	cout<<buffer<<endl;
	send(socket,buffer,strlen(buffer),0);
}

void send_clients_for_file(string status,string cid,string gid,string filename,int socket)
{
    //l:success:gid:hash:size:ip,port:ip,port:
	string reply="l:success:" + gid + ":";
	auto it1 = hashvalue.find(filename);
	if(it1 != hashvalue.end())
	{
		string hash = (it1->second).first;
		pair<string,string> p;
		p = (it1->second).second;
		reply = reply + hash + ":" + p.second + ":";
		auto it2 = client_list.find(hash);
		if(it2 != client_list.end())
		{
			list<string> clients = it2->second;
			auto it3 = clients.begin();//send ip port of clients and not client ids you fool
			while(it3 != clients.end())
			{
				string cidtemp = *it3;
				auto it4 = user_details.find(cidtemp);
				if(it4 != user_details.end())
				{
                    reply = reply + (it4->second).first + "," + (it4->second).second +":";
				}else{
                    cout<<" cid "<<cidtemp<<" not found "<<endl;
				}
				it3++;
			}
			(it2->second).push_back(cid);
		}else{
            cout<<" did not find hash in client list "<<endl;
		}
		char buffer[reply.size()+1];
        strcpy(buffer,reply.c_str());
        cout<<buffer<<endl;
        send(socket,buffer,strlen(buffer),0);
	}else{
        cout<<"Did not find hash value for the given filename "<<endl;
	}
}

void store_file_upload(string status,string cid,string gid,string filename,string sha1,string filesize,int socket)
{
	string reply="n:success:";
	cout<<"1";
	hashvalue[filename] = make_pair(sha1,make_pair(gid,filesize));
	cout<<"2";
	list<string> temp;
	cout<<"3";
	temp.push_back(cid);
	cout<<"4";
	client_list[sha1] = temp;
	cout<<"5";
	auto it = files_in_group.find(gid);
	cout<<"6";
	if(it != files_in_group.end())
	{
		(it->second).push_back(filename);
		cout<<"7";
	}else{
        cout<<"Group not found"<<endl;
	}
	char buffer[reply.size()+1];
	cout<<"1";
	strcpy(buffer,reply.c_str());
	cout<<"1";
	cout<<buffer<<endl;
	cout<<"1";
	send(socket,buffer,reply.size()+1,0);
}

void create_group(string status,string cid,string gid,string ip,string port,int socket)
{
    string reply = "p:success:";
    owner_of_group[gid]=make_pair(ip,port);
    list<string> temp;
    temp.push_back(cid);
    group_membership[gid]=temp;
    list<string> temp1;
    files_in_group[gid]=temp1;
    char buffer[reply.size()+1];
	strcpy(buffer,reply.c_str());
	cout<<buffer<<endl;
	send(socket,buffer,strlen(buffer),0);
}

void leave_group(string status,string gid,string cid, int socket)
{
    auto it = group_membership.find(gid);
    if(it != group_membership.end())
    {
        (it->second).remove(cid);
    }
    for(auto it1 = client_list.begin(); it1 != client_list.end(); it1++)
    {
        (it1->second).remove(cid);
    }
    string reply = "success";
    char buffer[reply.size()+1];
	strcpy(buffer,reply.c_str());
	cout<<buffer<<endl;
	send(socket,buffer,strlen(buffer),0);
}

void group_member_addition(string status,string gid,string cid,int socket)
{
    auto it = group_membership.find(gid);
    if(it != group_membership.end())
    {
        (it->second).push_back(cid);
        cout<<"Added "<<cid<<" to group "<<gid<<endl;
        string reply = "success:";
        char buffer[reply.size()+1];
        strcpy(buffer,reply.c_str());
        cout<<buffer<<endl;
        send(socket,buffer,strlen(buffer),0);
    }else{
        cout<<"group not found"<<endl;
    }
}

void* parseInput(void *vargp)
{
    int valread;
    char buffer[1024] = {0};
    int *temp = (int *)vargp;
    int socket = *temp;
    cout<<"in thread "<<getpid()<<endl;
    valread = recv( socket , buffer, 1024,0);
    printf("%d\n",valread );
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
    cout<<buffer<<"hihiihi";
    char *token = strtok_r(buffer,":",&saveptr1);
    switch(msg_type)
    {
    	case 'a':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(cid,strtok_r(NULL,":",&saveptr1));
    		strcpy(password,strtok_r(NULL,":",&saveptr1));
    		strcpy(ip,strtok_r(NULL,":",&saveptr1));
    		strcpy(port,strtok_r(NULL,":",&saveptr1));
       		create_account(status,cid,password,ip,port,socket);
    		break;
    	}
    	case 'c':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(cid,strtok_r(NULL,":",&saveptr1));
    		strcpy(password,strtok_r(NULL,":",&saveptr1));
    		strcpy(ip,strtok_r(NULL,":",&saveptr1));
    		strcpy(port,strtok_r(NULL,":",&saveptr1));
    		login_account(status,cid,password,ip,port,socket);
    		break;
    	}
    	case 'e':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		send_group_list(status,socket);
    		break;
    	}
    	case 'g':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		send_group_owner(status,gid,socket);
    		break;
    	}
    	case 'i':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		send_file_list(status,gid,socket);
    		break;
    	}
    	case 'k':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(cid,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		strcpy(filename,strtok_r(NULL,":",&saveptr1));
    		send_clients_for_file(status,cid,gid,filename,socket);
    		break;
    	}
    	case 'm':
    	{
    		strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(cid,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		strcpy(filename,strtok_r(NULL,":",&saveptr1));
    		strcpy(sha1,strtok_r(NULL,":",&saveptr1));
    		strcpy(filesize,strtok_r(NULL,":",&saveptr1));
    		store_file_upload(status,cid,gid,filename,sha1,filesize,socket);
    		break;
    	}
    	case 'o':
    	{
            strcpy(status,strtok_r(NULL,":",&saveptr1));
            strcpy(cid,strtok_r(NULL,":",&saveptr1));
            strcpy(gid,strtok_r(NULL,":",&saveptr1));
            strcpy(ip,strtok_r(NULL,":",&saveptr1));
            strcpy(port,strtok_r(NULL,":",&saveptr1));
            create_group(status,cid,gid,ip,port,socket);
            break;
    	}
    	case 'q':
    	{
            strcpy(status,strtok_r(NULL,":",&saveptr1));
            strcpy(gid,strtok_r(NULL,":",&saveptr1));
            strcpy(cid,strtok_r(NULL,":",&saveptr1));
            leave_group(status,gid,cid,socket);
            break;
    	}
    	case '0':
    	{
            strcpy(status,strtok_r(NULL,":",&saveptr1));
    		strcpy(gid,strtok_r(NULL,":",&saveptr1));
    		strcpy(cid,strtok_r(NULL,":",&saveptr1));
    		group_member_addition(status,gid,cid,socket);
    		break;
    	}
    	default :
    	{
    		cout<<"Unidentified message type "<<msg_type<<endl;
    		break;
    	}
    }
    return NULL;
}
void * listenerThread(void * arg)
{
    int server_fd, new_socket, valread;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[1024] = {0};
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
	address.sin_port = htons( TPORT );

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


void insertDummyargs()
{
    string ip1 = "127.0.0.1";
	string gid1 = "12345";
	string gid2 = "12346";
	string gid3 = "12347";
	string gid4 = "12348";
	string gid5 = "12349";
	string port1 = "9000";
    string port2 = "9001";
    string port3 = "9002";
    string port4 = "9003";
    string port5 = "9004";
    string port6 = "9005";
    string port7 = "9006";
    string port8 = "9007";
    string port9 = "9008";
    string port10 = "9009";
    string port11 = "9010";
    string port12 = "9011";
    string port13 = "9012";
    string port14 = "9013";
    string port15 = "9014";
	pair<string,string> p1 = make_pair(ip1,port1);
	pair<string,string> p2 = make_pair(ip1,port2);
	pair<string,string> p3 = make_pair(ip1,port3);
	pair<string,string> p4 = make_pair(ip1,port4);
	pair<string,string> p5 = make_pair(ip1,port5);
	owner_of_group[gid1]=p1;
    owner_of_group[gid2]=p2;
    owner_of_group[gid3]=p3;
    owner_of_group[gid4]=p4;
    owner_of_group[gid5]=p5;
    string cid1 = "10000";      //owner
    string cid2 = "10001";      //owner
    string cid3 = "10002";      //owner
    string cid4 = "10003";      //owner
    string cid5 = "10004";      //owner
    string cid6 = "10005";
    string cid7 = "10006";
    string cid8 = "10007";
    string cid9 = "10008";
    string cid10 = "10009";
    string cid11 = "10010";
    string cid12 = "10011";
    string cid13 = "10012";
    string cid14 = "10013";
    string cid15 = "10014";
    string pass1 = "pass1";
    string pass2 = "pass1";
    string pass3 = "pass2";
    string pass4 = "pass3";
    string pass5 = "pass4";
    string pass6 = "pass5";
    string pass7 = "pass6";
    string pass8 = "pass8";
    string pass9 = "pass9";
    string pass10 = "pass10";
    string pass11 = "pass11";
    string pass12 = "pass12";
    string pass13 = "pass13";
    string pass14 = "pass14";
    string pass15 = "pass15";
    string fname1 = "a.txt";
    string fname2 = "b.txt";
    string fname3 = "c.txt";
    string fname4 = "d.txt";
    string fname5 = "e.txt";
    string fname6 = "f.txt";
    string fname7 = "g.txt";
    string fname8 = "h.txt";
    string fname9 = "i.txt";
    string fname10 = "j.txt";
    string fname11 = "k.txt";
    string fname12 = "l.txt";
    string fname13 = "m.txt";
    string fname14 = "n.txt";
    string fname15 = "o.txt";
    string fname16 = "p.txt";
    string fname17 = "q.txt";
    string fname18 = "r.txt";
    string fname19 = "s.txt";
    string fname20 = "t.txt";
    string fname21 = "u.txt";
    string fname22 = "v.txt";
    ulli size1 = 1073741824;
    ulli size15 = 1610612736;
    ulli size2 = 2147483648;
    string hash1 = "hash_a.txt_file12345";
    string hash2 = "hash_b.txt_file12345";
    string hash3 = "hash_c.txt_file12345";
    string hash4 = "hash_d.txt_file12345";
    string hash5 = "hash_e.txt_file12345";
    string hash6 = "hash_f.txt_file12345";
    string hash7 = "hash_g.txt_file12345";
    string hash8 = "hash_h.txt_file12345";
    string hash9 = "hash_i.txt_file12345";
    string hash10 = "hash_j.txt_file12345";
    string hash11 = "hash_k.txt_file12345";
    string hash12 = "hash_l.txt_file12345";
    string hash13 = "hash_m.txt_file12345";
    string hash14 = "hash_n.txt_file12345";
    string hash15 = "hash_o.txt_file12345";
    string hash16 = "hash_p.txt_file12345";
    string hash17 = "hash_q.txt_file12345";
    string hash18 = "hash_r.txt_file12345";
    string hash19 = "hash_s.txt_file12345";
    string hash20 = "hash_t.txt_file12345";
    string hash21 = "hash_u.txt_file12345";
    string hash22 = "hash_v.txt_file12345";
    //id    ip,port
    user_details[cid1]=make_pair(ip1,port1);
    user_details[cid2]=make_pair(ip1,port2);
    user_details[cid3]=make_pair(ip1,port3);
    user_details[cid4]=make_pair(ip1,port4);
    user_details[cid5]=make_pair(ip1,port5);
    user_details[cid6]=make_pair(ip1,port6);
    user_details[cid7]=make_pair(ip1,port7);
    user_details[cid8]=make_pair(ip1,port8);
    user_details[cid9]=make_pair(ip1,port9);
    user_details[cid10]=make_pair(ip1,port10);
    user_details[cid11]=make_pair(ip1,port11);
    user_details[cid12]=make_pair(ip1,port12);
    user_details[cid13]=make_pair(ip1,port13);
    user_details[cid14]=make_pair(ip1,port14);
    user_details[cid15]=make_pair(ip1,port15);
    //id pass
    authentication[cid1]=pass1;
    authentication[cid2]=pass2;
    authentication[cid3]=pass3;
    authentication[cid4]=pass4;
    authentication[cid5]=pass5;
    authentication[cid6]=pass6;
    authentication[cid7]=pass7;
    authentication[cid8]=pass8;
    authentication[cid9]=pass9;
    authentication[cid10]=pass10;
    authentication[cid11]=pass11;
    authentication[cid12]=pass12;
    authentication[cid13]=pass13;
    authentication[cid14]=pass14;
    authentication[cid15]=pass15;
    //fname     hash, <cid,size>
    hashvalue[fname1]=make_pair(hash1,make_pair(cid1,size1));
    hashvalue[fname2]=make_pair(hash2,make_pair(cid2,size15));
    hashvalue[fname3]=make_pair(hash3,make_pair(cid3,size1));
    hashvalue[fname4]=make_pair(hash4,make_pair(cid4,size2));
    hashvalue[fname5]=make_pair(hash5,make_pair(cid5,size1));
    hashvalue[fname6]=make_pair(hash6,make_pair(cid6,size15));
    hashvalue[fname7]=make_pair(hash7,make_pair(cid7,size1));
    hashvalue[fname8]=make_pair(hash8,make_pair(cid8,size1));
    hashvalue[fname9]=make_pair(hash9,make_pair(cid9,size2));
    hashvalue[fname10]=make_pair(hash10,make_pair(cid10,size2));
    hashvalue[fname11]=make_pair(hash11,make_pair(cid11,size15));
    hashvalue[fname12]=make_pair(hash12,make_pair(cid12,size1));
    hashvalue[fname13]=make_pair(hash13,make_pair(cid13,size2));
    hashvalue[fname14]=make_pair(hash14,make_pair(cid14,size15));
    hashvalue[fname15]=make_pair(hash15,make_pair(cid3,size2));
    hashvalue[fname16]=make_pair(hash16,make_pair(cid1,size1));
    hashvalue[fname17]=make_pair(hash17,make_pair(cid1,size15));
    hashvalue[fname18]=make_pair(hash18,make_pair(cid2,size2));
    hashvalue[fname19]=make_pair(hash19,make_pair(cid7,size1));
    hashvalue[fname20]=make_pair(hash20,make_pair(cid5,size1));
    hashvalue[fname21]=make_pair(hash21,make_pair(cid4,size2));
    hashvalue[fname22]=make_pair(hash22,make_pair(cid10,size15));
    //hash      listofcid
    list<string> list1;
    list<string> list2;
    list<string> list3;
    list<string> list4;
    list<string> list5;
    list<string> list6;
    list<string> list7;
    list<string> list8;
    list<string> list9;
    list<string> list10;
    list<string> list11;
    list<string> list12;
    list<string> list13;
    list<string> list14;
    list<string> list15;
    list<string> list16;
    list<string> list17;
    list<string> list18;
    list<string> list19;
    list<string> list20;
    list<string> list21;
    list<string> list22;
    list1.push_back(cid1);
    list1.push_back(cid6);
    list1.push_back(cid14);
    list6.push_back(cid6);
    list6.push_back(cid12);
    list12.push_back(cid12);
    list14.push_back(cid14);
    list16.push_back(cid1);
    list16.push_back(cid6);
    list17.push_back(cid1);
    list17.push_back(cid14);
    list2.push_back(cid2);
    list2.push_back(cid9);
    list2.push_back(cid13);
    list9.push_back(cid9);
    list13.push_back(cid13);
    list13.push_back(cid9);
    list18.push_back(cid2);
    list3.push_back(cid3);
    list3.push_back(cid8);
    list8.push_back(cid8);
    list5.push_back(cid5);
    list5.push_back(cid10);
    list10.push_back(cid10);
    list11.push_back(cid11);
    list11.push_back(cid10);
    list20.push_back(cid5);
    list22.push_back(cid10);
    list4.push_back(cid4);
    list7.push_back(cid7);
    list15.push_back(cid15);
    list15.push_back(cid7);
    list15.push_back(cid4);
    list19.push_back(cid7);
    list21.push_back(cid4);
    client_list[hash1]=list1;
    client_list[hash2]=list2;
    client_list[hash3]=list3;
    client_list[hash4]=list4;
    client_list[hash5]=list5;
    client_list[hash6]=list6;
    client_list[hash7]=list7;
    client_list[hash8]=list8;
    client_list[hash9]=list9;
    client_list[hash10]=list10;
    client_list[hash11]=list11;
    client_list[hash12]=list12;
    client_list[hash13]=list13;
    client_list[hash14]=list14;
    client_list[hash15]=list15;
    client_list[hash16]=list16;
    client_list[hash17]=list17;
    client_list[hash18]=list18;
    client_list[hash19]=list19;
    client_list[hash20]=list20;
    client_list[hash21]=list21;
    client_list[hash22]=list22;
    list<string> flist1;
    list<string> flist2;
    list<string> flist4;
    list<string> flist5;
    list<string> flist3;
    flist1.push_back(hash1);
    flist1.push_back(hash16);
    flist1.push_back(hash17);
    flist1.push_back(hash6);
    flist1.push_back(hash12);
    flist1.push_back(hash14);
    flist2.push_back(hash2);
    flist2.push_back(hash18);
    flist2.push_back(hash9);
    flist2.push_back(hash13);
    flist3.push_back(hash3);
    flist3.push_back(hash8);
    flist4.push_back(hash4);
    flist4.push_back(hash21);
    flist4.push_back(hash7);
    flist4.push_back(hash15);
    flist5.push_back(hash5);
    flist5.push_back(hash20);
    flist5.push_back(hash10);
    flist5.push_back(hash11);
    flist5.push_back(hash22);
    files_in_group[gid1]=flist1;
    files_in_group[gid2]=flist2;
    files_in_group[gid3]=flist3;
    files_in_group[gid4]=flist4;
    files_in_group[gid5]=flist5;

}

int main(int argc, char const *argv[])
{
    //insertDummyargs();
    KEEP_LISTENING = true;
    pthread_t listenerThreadid;
    pthread_create(&listenerThreadid, NULL, listenerThread,NULL);
    pthread_join(listenerThreadid,NULL);
    return 0;
}

