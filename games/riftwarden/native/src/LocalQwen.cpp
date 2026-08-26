#include "LocalQwen.h"
#include <windows.h>
#include <winhttp.h>
#include <regex>
#include <sstream>
#include <vector>

static std::string ReadResponse(HINTERNET request){std::string out;DWORD available=0;do{if(!WinHttpQueryDataAvailable(request,&available))break;std::vector<char> data(available);DWORD read=0;if(available&&!WinHttpReadData(request,data.data(),available,&read))break;out.append(data.data(),read);}while(available);return out;}
static std::string JsonEscape(const std::string& value){std::string out;out.reserve(value.size()+32);for(char c:value){if(c=='\\')out+="\\\\";else if(c=='\"')out+="\\\"";else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else out+=c;}return out;}
static std::string Unescape(std::string value){std::string out;for(size_t i=0;i<value.size();++i){if(value[i]=='\\'&&i+1<value.size()){char n=value[++i];if(n=='n')out+='\n';else if(n=='r'){}else if(n=='t')out+='\t';else if(n=='"')out+='"';else if(n=='\\')out+='\\';else{out+='\\';out+=n;}}else out+=value[i];}return out;}
static std::string MatchString(const std::string& source,const char* field){std::regex r(std::string("\\\"")+field+"\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");std::smatch m;return std::regex_search(source,m,r)?m[1].str():"";}
static float MatchFloat(const std::string& source,const char* field,float fallback){std::regex r(std::string("\\\"")+field+"\\\"\\s*:\\s*([0-9.]+)");std::smatch m;return std::regex_search(source,m,r)?std::stof(m[1].str()):fallback;}

std::optional<GeneratedEvent> LocalQwen::Generate(const BattleState& s)const{
    HINTERNET session=WinHttpOpen(L"Riftwarden/1.0",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session)return{};
    WinHttpSetTimeouts(session,3000,3000,3000,15000);HINTERNET connect=WinHttpConnect(session,L"127.0.0.1",8081,0);if(!connect){WinHttpCloseHandle(session);return{};}
    HINTERNET request=WinHttpOpenRequest(connect,L"POST",L"/v1/chat/completions",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0);if(!request){WinHttpCloseHandle(connect);WinHttpCloseHandle(session);return{};}
    std::ostringstream prompt;prompt<<"You direct events for a tower defense game. State: wave="<<s.wave<<", lives="<<s.lives<<", gold="<<s.gold<<", towers="<<s.towers<<", kills="<<s.kills<<". Return ONLY JSON: {\"title\":\"short English title\",\"description\":\"short description\",\"type\":\"gold_rush|enemy_haste|fortify|fog|bounty|mana_storm\",\"magnitude\":0.1 to 0.35,\"duration\":6 to 16}.";
    std::string body="{\"model\":\"Qwen3.8-27B-UD-Q4_K_M.gguf\",\"temperature\":0.7,\"max_tokens\":180,\"chat_template_kwargs\":{\"enable_thinking\":false,\"preserve_thinking\":false},\"messages\":[{\"role\":\"user\",\"content\":\""+JsonEscape(prompt.str())+"\"}]}";
    const wchar_t* headers=L"Content-Type: application/json\r\nAuthorization: Bearer riftwarden-local-only\r\n";BOOL ok=WinHttpSendRequest(request,headers,DWORD(-1),body.data(),DWORD(body.size()),DWORD(body.size()),0)&&WinHttpReceiveResponse(request,nullptr);std::string response=ok?ReadResponse(request):"";WinHttpCloseHandle(request);WinHttpCloseHandle(connect);WinHttpCloseHandle(session);if(response.empty())return{};
    std::regex contentRegex("\\\"content\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"");std::smatch cm;if(!std::regex_search(response,cm,contentRegex))return{};std::string inner=Unescape(cm[1].str());auto begin=inner.find('{'),end=inner.rfind('}');if(begin==std::string::npos||end==std::string::npos)return{};inner=inner.substr(begin,end-begin+1);
    GeneratedEvent event;event.title=MatchString(inner,"title");event.description=MatchString(inner,"description");event.type=MatchString(inner,"type");event.magnitude=(std::max)(.1f,(std::min)(.35f,MatchFloat(inner,"magnitude",.2f)));event.duration=int((std::max)(6.f,(std::min)(16.f,MatchFloat(inner,"duration",10))));
    if(event.type!="gold_rush"&&event.type!="enemy_haste"&&event.type!="fortify"&&event.type!="fog"&&event.type!="bounty"&&event.type!="mana_storm")return{};return event;
}
