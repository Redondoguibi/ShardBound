#include "scripting/Syntax.h"
#include <algorithm>
#include <cctype>
#include <sstream>
namespace game::script {
std::vector<Token> lex(std::string_view source,const std::string& file) {
    std::vector<Token> tokens;
    std::vector<std::pair<unsigned,std::string>> indents{{0,""}};
    std::istringstream stream{std::string(source)}; std::string line; unsigned row=0;
    bool comment=false; int parens=0;
    while (std::getline(stream,line)) {
        ++row;
        if (!line.empty() && line.back()=='\r') line.pop_back();
        // Blank comment characters preserve columns and indentation diagnostics.
        bool quoted=false,escaped=false;
        for(size_t i=0;i<line.size();++i) {
            if(comment) {
                if(line[i]=='?' && i+1<line.size() && line[i+1]=='/') { line[i]=line[i+1]=' '; ++i; comment=false; }
                else line[i]=' ';
            } else if(quoted) {
                if(escaped) escaped=false;
                else if(line[i]=='\\') escaped=true;
                else if(line[i]=='"') quoted=false;
            } else if(line[i]=='"') quoted=true;
            else if(line[i]=='/' && i+1<line.size() && line[i+1]=='?') { line[i]=line[i+1]=' '; ++i; comment=true; }
            else if(line[i]=='?') { std::fill(line.begin()+static_cast<std::ptrdiff_t>(i),line.end(),' '); break; }
        }
        size_t i=0; unsigned indent=0;
        while(i<line.size() && (line[i]==' ' || line[i]=='\t')) { indent+=line[i]=='\t' ? 4 : 1; ++i; }
        if(i==line.size()) continue;
        auto emit=[&](TokenKind kind,std::string text,size_t column) { tokens.push_back({kind,std::move(text),{file,row,static_cast<unsigned>(column+1)}}); };
        if(parens==0) {
            const auto prefix=line.substr(0,i);
            if(prefix.find(' ')!=std::string::npos && prefix.find('\t')!=std::string::npos) fail({file,row,1},"SCRIPT101","Mixed tabs and spaces in indentation");
            if(indent>indents.back().first) { indents.emplace_back(indent,prefix); emit(TokenKind::Indent,"<indent>",0); }
            else {
                while(indent<indents.back().first) { indents.pop_back(); emit(TokenKind::Dedent,"<dedent>",0); }
                if(indent!=indents.back().first || prefix!=indents.back().second) fail({file,row,1},"SCRIPT101","Inconsistent indentation");
            }
        }
        while(i<line.size()) {
            const char c=line[i];
            if(c==' ' || c=='\t') { ++i; continue; }
            const auto start=i;
            if(std::isalpha(static_cast<unsigned char>(c)) || c=='_') {
                while(i<line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i]=='_')) ++i;
                emit(TokenKind::Name,line.substr(start,i-start),start);
            } else if(std::isdigit(static_cast<unsigned char>(c))) {
                while(i<line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
                if(i<line.size() && line[i]=='.' && i+1<line.size() && std::isdigit(static_cast<unsigned char>(line[i+1]))) { ++i; while(i<line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) ++i; }
                emit(TokenKind::Number,line.substr(start,i-start),start);
            } else if(c=='"') {
                ++i; std::string value; bool closed=false;
                while(i<line.size()) {
                    char next=line[i++]; if(next=='"') { closed=true; break; }
                    if(next=='\\') {
                        if(i==line.size()) break;
                        next=line[i++];
                        if(next=='n') next='\n'; else if(next=='t') next='\t'; else if(next!='"' && next!='\\') fail({file,row,static_cast<unsigned>(i)},"SCRIPT101","Unsupported string escape");
                    }
                    value+=next;
                }
                if(!closed) fail({file,row,static_cast<unsigned>(start+1)},"SCRIPT101","Unterminated string");
                emit(TokenKind::String,std::move(value),start);
            } else {
                std::string text(1,c); ++i;
                if(i<line.size() && line[i]=='=' && (c=='=' || c=='!' || c=='<' || c=='>')) text+=line[i++];
                if(std::string_view("@.:,{}()[]+-*/%=!<>").find(c)==std::string_view::npos) fail({file,row,static_cast<unsigned>(start+1)},"SCRIPT101","Unexpected character");
                if(c=='(' || c=='[') ++parens;
                if(c==')' || c==']') { --parens; if(parens<0) fail({file,row,static_cast<unsigned>(start+1)},"SCRIPT101","Unmatched closing delimiter"); }
                emit(TokenKind::Punctuation,std::move(text),start);
            }
        }
        if(parens==0) emit(TokenKind::Newline,"<newline>",line.size());
    }
    if(comment || parens) fail({file,row,1},"SCRIPT101","Unterminated comment or delimiter");
    while(indents.size()>1) { indents.pop_back(); tokens.push_back({TokenKind::Dedent,"<dedent>",{file,row+1,1}}); }
    tokens.push_back({TokenKind::End,"<end>",{file,row+1,1}}); return tokens;
}
}
