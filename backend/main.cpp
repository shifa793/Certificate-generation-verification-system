#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <mysql.h>
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <map>
#include <vector>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <cctype>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

const int PORT = 8080;
const char* DB_HOST = "127.0.0.1";
const char* DB_USER = "root";
const char* DB_PASS = ""; // Set your MySQL password here if you have one.
const char* DB_NAME = "certificate_system";

string jsonEscape(const string& s) {
    string out;
    for (unsigned char c : s) {
        switch (c) { case '"': out += "\\\""; break; case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break;
            default: if (c < 32) {} else out += static_cast<char>(c); }
    }
    return out;
}
string urlDecode(const string& s) {
    string out;
    for (size_t i=0; i<s.size(); ++i) {
        if (s[i]=='+') out+=' ';
        else if (s[i]=='%' && i+2<s.size()) {
            try { out += static_cast<char>(stoi(s.substr(i+1,2), nullptr, 16)); i+=2; }
            catch (...) { out += s[i]; }
        } else out += s[i];
    }
    return out;
}
map<string,string> parseForm(const string& body) {
    map<string,string> fields; stringstream ss(body); string item;
    while (getline(ss,item,'&')) { auto p=item.find('='); if(p!=string::npos) fields[urlDecode(item.substr(0,p))]=urlDecode(item.substr(p+1)); }
    return fields;
}
string today() {
    time_t t=time(nullptr); tm local{};
#ifdef _WIN32
    localtime_s(&local,&t);
#else
    localtime_r(&t,&local);
#endif
    stringstream s; s<<put_time(&local,"%Y-%m-%d"); return s.str();
}
string makeId() {
    // ID is generated from time plus a short random-ish suffix; primary key collision is checked by MySQL.
    srand(static_cast<unsigned int>(time(nullptr)) ^ static_cast<unsigned int>(GetTickCount()));
    stringstream s; s<<"CERT-"<<time(nullptr)<<"-"<<uppercase<<hex<<(rand()%65536); return s.str();
}
MYSQL* connectDb() {
    MYSQL* db=mysql_init(nullptr);
    if(!db) return nullptr;
    mysql_options(db, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    // XAMPP's local MySQL/MariaDB commonly runs without TLS. Explicitly
    // tell the MariaDB Connector/C client not to require SSL.
    my_bool ssl_enforce = 0;
    my_bool ssl_verify = 0;
    if (mysql_options(db, MYSQL_OPT_SSL_ENFORCE, &ssl_enforce) != 0) {
        cerr << "Warning: could not set MYSQL_OPT_SSL_ENFORCE: " << mysql_error(db) << "\n";
    }
    if (mysql_options(db, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &ssl_verify) != 0) {
        cerr << "Warning: could not disable SSL certificate verification: " << mysql_error(db) << "\n";
    }

    if(!mysql_real_connect(db,DB_HOST,DB_USER,DB_PASS,DB_NAME,3306,nullptr,0)) { cerr<<"MySQL: "<<mysql_error(db)<<"\n"; mysql_close(db); return nullptr; }
    return db;
}
string response(const string& status,const string& type,const string& body) {
    stringstream s; s<<"HTTP/1.1 "<<status<<"\r\nContent-Type: "<<type<<"; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\n\r\n"<<body; return s.str();
}
string getParam(const string& target,const string& key) {
    auto q=target.find('?'); if(q==string::npos) return "";
    auto fields=parseForm(target.substr(q+1)); auto it=fields.find(key); return it==fields.end()?"":it->second;
}
string handleVerify(const string& id) {
    if(id.empty()) return "{\"success\":false,\"message\":\"Enter a certificate ID.\"}";
    MYSQL* db=connectDb(); if(!db) return "{\"success\":false,\"message\":\"Database connection failed. Check backend console and DB settings.\"}";
    const char* q="SELECT certificate_id, student_name, course_name, issue_date, authorized_name FROM certificates WHERE certificate_id=? LIMIT 1";
    MYSQL_STMT* st=mysql_stmt_init(db);
    if(!st || mysql_stmt_prepare(st,q,strlen(q))!=0) { if(st) mysql_stmt_close(st); mysql_close(db); return "{\"success\":false,\"message\":\"Could not prepare verification query.\"}"; }
    MYSQL_BIND p[1]{}; unsigned long idLen=static_cast<unsigned long>(id.size()); p[0].buffer_type=MYSQL_TYPE_STRING; p[0].buffer=(void*)id.c_str(); p[0].buffer_length=idLen; p[0].length=&idLen;
    if(mysql_stmt_bind_param(st,p)!=0 || mysql_stmt_execute(st)!=0 || mysql_stmt_store_result(st)!=0) { mysql_stmt_close(st); mysql_close(db); return "{\"success\":false,\"message\":\"Verification query failed.\"}"; }
    if(mysql_stmt_num_rows(st)==0) { mysql_stmt_free_result(st); mysql_stmt_close(st); mysql_close(db); return "{\"success\":true,\"valid\":false,\"message\":\"Certificate not found or invalid.\"}"; }
    char idBuf[51]{},nameBuf[101]{},courseBuf[151]{},dateBuf[11]{},authBuf[101]{}; unsigned long lens[5]{}; my_bool nulls[5]{};
    MYSQL_BIND r[5]{}; char* bufs[]={idBuf,nameBuf,courseBuf,dateBuf,authBuf}; unsigned long caps[]={50,100,150,10,100};
    for(int i=0;i<5;i++){ r[i].buffer_type=MYSQL_TYPE_STRING; r[i].buffer=bufs[i]; r[i].buffer_length=caps[i]; r[i].length=&lens[i]; r[i].is_null=&nulls[i]; }
    mysql_stmt_bind_result(st,r); mysql_stmt_fetch(st);
    string out="{\"success\":true,\"valid\":true,\"certificate\":{\"certificate_id\":\""+jsonEscape(idBuf)+"\",\"student_name\":\""+jsonEscape(nameBuf)+"\",\"course_name\":\""+jsonEscape(courseBuf)+"\",\"issue_date\":\""+jsonEscape(dateBuf)+"\",\"authorized_name\":\""+jsonEscape(authBuf)+"\"},\"message\":\"Certificate verified successfully.\"}";
    mysql_stmt_free_result(st); mysql_stmt_close(st); mysql_close(db); return out;
}
string handleGenerate(const string& body) {
    auto f=parseForm(body); string name=f["student_name"], course=f["course_name"], auth=f["authorized_name"];
    auto trim=[](string& x){ while(!x.empty() && isspace((unsigned char)x.front())) x.erase(x.begin()); while(!x.empty() && isspace((unsigned char)x.back())) x.pop_back(); };
    trim(name); trim(course); trim(auth);
    if(name.empty()||course.empty()||auth.empty()) return "{\"success\":false,\"message\":\"Student name, course name and authorized name are required.\"}";
    if(name.size()>100||course.size()>150||auth.size()>100) return "{\"success\":false,\"message\":\"One or more fields exceed the allowed length.\"}";
    MYSQL* db=connectDb(); if(!db) return "{\"success\":false,\"message\":\"Database connection failed. Check backend console and DB settings.\"}";
    string id=makeId(), date=today(); const char* q="INSERT INTO certificates (certificate_id,student_name,course_name,issue_date,authorized_name) VALUES (?,?,?,?,?)";
    MYSQL_STMT* st=mysql_stmt_init(db);
    if(!st||mysql_stmt_prepare(st,q,strlen(q))!=0){if(st)mysql_stmt_close(st);mysql_close(db);return "{\"success\":false,\"message\":\"Could not prepare certificate insert.\"}";}
    string vals[]={id,name,course,date,auth}; MYSQL_BIND b[5]{}; unsigned long lens[5];
    for(int i=0;i<5;i++){lens[i]=static_cast<unsigned long>(vals[i].size());b[i].buffer_type=MYSQL_TYPE_STRING;b[i].buffer=(void*)vals[i].c_str();b[i].buffer_length=lens[i];b[i].length=&lens[i];}
    if(mysql_stmt_bind_param(st,b)!=0||mysql_stmt_execute(st)!=0){string err=mysql_stmt_error(st);mysql_stmt_close(st);mysql_close(db);return "{\"success\":false,\"message\":\"Could not save certificate: "+jsonEscape(err)+"\"}";}
    mysql_stmt_close(st);mysql_close(db);
    return "{\"success\":true,\"certificate\":{\"certificate_id\":\""+jsonEscape(id)+"\",\"student_name\":\""+jsonEscape(name)+"\",\"course_name\":\""+jsonEscape(course)+"\",\"issue_date\":\""+date+"\",\"authorized_name\":\""+jsonEscape(auth)+"\"},\"message\":\"Certificate created and saved.\"}";
}
void serveClient(SOCKET client) {
    string req; char buf[4096]; int n;
    while((n=recv(client,buf,sizeof(buf),0))>0){req.append(buf,n); auto h=req.find("\r\n\r\n"); if(h!=string::npos){size_t len=0; auto cl=req.find("Content-Length:"); if(cl!=string::npos&&cl<h){cl+=15;while(cl<h&&req[cl]==' ')cl++;size_t e=req.find("\r\n",cl);try{len=stoul(req.substr(cl,e-cl));}catch(...){}} if(req.size()>=h+4+len)break;} if(req.size()>1024*1024)break;}
    string out; auto h=req.find("\r\n\r\n"); if(h==string::npos){out=response("400 Bad Request","application/json","{\"success\":false,\"message\":\"Bad request\"}");}
    else {
        string header=req.substr(0,h), body=req.substr(h+4); stringstream hs(header); string method,target,version; hs>>method>>target>>version;
        if(method=="OPTIONS") out=response("204 No Content","application/json","");
        else if(method=="GET"&&target.rfind("/api/verify",0)==0) out=response("200 OK","application/json",handleVerify(getParam(target,"id")));
        else if(method=="POST"&&target=="/api/generate") out=response("200 OK","application/json",handleGenerate(body));
        else if(method=="GET"&&target=="/api/health") out=response("200 OK","application/json","{\"success\":true,\"message\":\"C++ certificate API is running\"}");
        else out=response("404 Not Found","application/json","{\"success\":false,\"message\":\"API route not found\"}");
    }
    send(client,out.c_str(),static_cast<int>(out.size()),0); shutdown(client,SD_SEND); closesocket(client);
}
int main(){
    WSADATA wsa{}; if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){cerr<<"Winsock initialization failed.\n";return 1;}
    SOCKET server=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); if(server==INVALID_SOCKET){cerr<<"Could not create server socket.\n";WSACleanup();return 1;}
    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_ANY);addr.sin_port=htons(PORT);
    if(bind(server,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==SOCKET_ERROR||listen(server,10)==SOCKET_ERROR){cerr<<"Could not start server on port "<<PORT<<". Is another app using it?\n";closesocket(server);WSACleanup();return 1;}
    cout<<"Certificate System C++ API running at http://localhost:"<<PORT<<"\n";
    cout<<"Keep this terminal open. Press Ctrl+C to stop.\n";
    while(true){SOCKET client=accept(server,nullptr,nullptr);if(client==INVALID_SOCKET){cerr<<"Accept failed.\n";continue;}serveClient(client);}
    closesocket(server);WSACleanup();return 0;
}
