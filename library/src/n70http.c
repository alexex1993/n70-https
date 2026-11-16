#include "n70http.h"
#include <string.h>
#include <stdio.h>
enum { H_HEADER,H_LENGTH,H_CLOSE,H_SIZE,H_CHUNK,H_CRLF,H_TRAILER,H_END };
static int lower(int c) { return c>='A'&&c<='Z'?c+32:c; }
static int equal(const char*a,const char*b)
{ while(*a&&*b)if(lower(*a++)!=lower(*b++))return 0;return !*a&&!*b; }
int n70_header_value(const char*raw,size_t len,const char*name,size_t nlen,const char**value,size_t*vlen)
{
    size_t pos=0,end,i,colon;
    *value=NULL;*vlen=0;if(!nlen)return 0;
    while(pos+1<len&&(raw[pos]!='\r'||raw[pos+1]!='\n'))++pos;
    pos+=2;
    while(pos<len){
        end=pos;while(end+1<len&&(raw[end]!='\r'||raw[end+1]!='\n'))++end;
        if(end+1>=len||end==pos)return 0;
        colon=pos;while(colon<end&&raw[colon]!=':')++colon;
        if(colon<end&&colon-pos==nlen){
            for(i=0;i<nlen;i++)if(lower((unsigned char)raw[pos+i])!=lower((unsigned char)name[i]))break;
            if(i==nlen){
                pos=colon+1;while(pos<end&&(raw[pos]==' '||raw[pos]=='\t'))++pos;
                while(end>pos&&(raw[end-1]==' '||raw[end-1]=='\t'))--end;
                *value=raw+pos;*vlen=end-pos;return 1;
            }
        }
        pos=end+2;
    }
    return 0;
}
static int token(int c)
{ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||strchr("!#$%&'*+-.^_`|~",c)!=NULL; }
static int field(char*line,char**value)
{
    char*p=line,*v,*end;
    if(!*p)return 0;
    while(*p&&*p!=':'){if(!token((unsigned char)*p))return 0;++p;}
    if(*p!=':')return 0;*p++=0;
    while(*p==' '||*p=='\t')++p;v=p;
    for(;*p;++p)if(((unsigned char)*p<32&&*p!='\t')||(unsigned char)*p==127)return 0;
    end=p;while(end>v&&(end[-1]==' '||end[-1]=='\t'))--end;*end=0;*value=v;return 1;
}
static int headers(n70_http*p)
{
    char copy[N70_HTTP_HEADERS_MAX+1],*line,*end,*v;
    int seen_te=0,seen_ce=0,seen_location=0,seen_retry=0;
    memcpy(copy,p->headers,p->hlen+1);line=copy;end=strstr(line,"\r\n");
    if(!end)return N70_BAD;*end=0;
    if(strlen(line)<12 || (strncmp(line,"HTTP/1.0 ",9)&&strncmp(line,"HTTP/1.1 ",9)) ||
       line[9]<'1'||line[9]>'5'||line[10]<'0'||line[10]>'9'||line[11]<'0'||line[11]>'9'||
       (line[12]&&line[12]!=' '))return N70_BAD;
    p->status=(line[9]-'0')*100+(line[10]-'0')*10+line[11]-'0';p->version11=line[7]=='1';
    p->connection_keepalive=p->self_delimited=0; /* close in an interim response also forbids reuse */
    for(v=line+12;*v;++v)if((unsigned char)*v<32||(unsigned char)*v==127)return N70_BAD;
    p->has_length=p->chunked=p->gzip=0;p->remaining=0;p->location[0]=0;
    line=end+2;
    while(*line){end=strstr(line,"\r\n");if(!end)return N70_BAD;*end=0;
        if(!*line)break;
        if(!field(line,&v))return N70_BAD;
        if(equal(line,"content-length")){
            uint64_t n=0;if(p->has_length||!*v)return N70_BAD;
            while(*v){unsigned d=(unsigned)(*v++-'0');if(d>9||n>1844674407370955161ULL||
                (n==1844674407370955161ULL&&d>5))return N70_BAD;n=n*10+d;}
            p->has_length=1;p->remaining=n;
        } else if(equal(line,"transfer-encoding")){
            if(seen_te++||!equal(v,"chunked"))return N70_ENCODING;p->chunked=1;
        } else if(equal(line,"content-encoding")){
            if(seen_ce++)return N70_ENCODING;
            if(equal(v,"gzip"))p->gzip=1;else if(!equal(v,"identity"))return N70_ENCODING;
        } else if(equal(line,"location")){
            if(seen_location++||strlen(v)>N70_URL_MAX)return N70_BAD;strcpy(p->location,v);
        } else if(equal(line,"retry-after")){
            if(seen_retry++)return N70_BAD;
        } else if(equal(line,"connection")){
            char*q=v,*start,*tail,*r;
            while(*q){while(*q==' '||*q=='\t'||*q==',')++q;if(!*q)break;
                start=q;while(*q&&*q!=',')++q;tail=q;while(tail>start&&(tail[-1]==' '||tail[-1]=='\t'))--tail;
                for(r=start;r<tail;++r)if(!token((unsigned char)*r))return N70_BAD;
                {char saved=*tail;*tail=0;
                 if(equal(start,"close"))p->connection_close=1;
                 if(equal(start,"keep-alive"))p->connection_keepalive=1;
                 *tail=saved;}
                if(*q)++q;
            }
        }
        line=end+2;
    }
    if(p->chunked&&p->has_length)return N70_BAD;
    if(p->status<200){
        if(p->status==101||++p->interim>8||p->chunked||p->has_length)return N70_BAD;
        p->hlen=0;p->state=H_HEADER;return N70_MORE;
    }
    p->self_delimited=p->head||p->status==204||p->status==304||p->chunked||p->has_length;
    if(p->head||p->status==204||p->status==304)p->state=H_END;
    else if(p->chunked)p->state=H_SIZE;
    else if(p->has_length){
        if(p->remaining>p->max_body)return N70_LIMIT;
        p->state=p->remaining?H_LENGTH:H_END;
    }else p->state=H_CLOSE;
    return N70_HEADERS;
}
void n70_http_init(n70_http*p,uint64_t limit,int head)
{ memset(p,0,sizeof *p);p->max_body=limit;p->head=head; }
int n70_http_feed(n70_http*p,const unsigned char*data,size_t len,size_t*used,n70_body_fn sink,void*ctx)
{
    size_t i=0;int result;*used=0;
    while(i<len){
        if(p->state==H_END){*used=i;return N70_DONE;}
        if(p->state==H_HEADER){
            unsigned c=data[i++];
            if(!c||++p->total_headers>65536||p->hlen>=N70_HTTP_HEADERS_MAX){*used=i;return N70_LIMIT;}
            p->headers[p->hlen++]=(char)c;p->headers[p->hlen]=0;
            if(p->hlen>=4&&!memcmp(p->headers+p->hlen-4,"\r\n\r\n",4)){
                result=headers(p);if(result!=N70_MORE){*used=i;return result;}
            }
        }else if(p->state==H_LENGTH||p->state==H_CHUNK||p->state==H_CLOSE){
            size_t n=len-i;
            if(p->state!=H_CLOSE && (uint64_t)n>p->remaining)n=(size_t)p->remaining;
            if((uint64_t)n>p->max_body-p->body_bytes){*used=i;return N70_LIMIT;}
            if(n&&sink&&sink(ctx,data+i,n)){*used=i;return N70_SINK;}
            p->body_bytes+=n;i+=n;
            if(p->state!=H_CLOSE){p->remaining-=n;if(!p->remaining){
                if(p->state==H_CHUNK){p->state=H_CRLF;p->crlf=0;}else p->state=H_END;
            }}
        }else if(p->state==H_CRLF){
            if(data[i++]!=(unsigned char)(p->crlf?'\n':'\r')){*used=i;return N70_BAD;}
            if(++p->crlf==2){p->state=H_SIZE;p->llen=0;}
        }else{
            unsigned c=data[i++];
            if(!c||p->llen>=sizeof p->line-1){*used=i;return N70_LIMIT;}
            if(p->state==H_TRAILER&&++p->trailer_bytes>8192){*used=i;return N70_LIMIT;}
            p->line[p->llen++]=(char)c;
            if(c=='\n'){
                char *v,*q;uint64_t n=0;int digits=0;
                if(p->llen<2||p->line[p->llen-2]!='\r'){*used=i;return N70_BAD;}
                p->line[p->llen-2]=0;
                if(p->state==H_TRAILER){
                    if(!p->line[0])p->state=H_END;
                    else if(!field(p->line,&v)||equal(p->line,"content-length")||equal(p->line,"transfer-encoding")||equal(p->line,"host")){
                        *used=i;return N70_BAD;
                    }
                }else{
                    q=p->line;
                    while(*q&&*q!=';'){
                        int d=(*q>='0'&&*q<='9')?*q-'0':(lower(*q)>='a'&&lower(*q)<='f'?lower(*q)-'a'+10:-1);
                        if(d<0||n>(UINT64_MAX>>4)){*used=i;return N70_BAD;}
                        n=n*16+(unsigned)d;++q;++digits;
                    }
                    if(!digits){*used=i;return N70_BAD;}
                    for(;*q;++q)if((unsigned char)*q<32||(unsigned char)*q==127){*used=i;return N70_BAD;}
                    if(n>p->max_body-p->body_bytes){*used=i;return N70_LIMIT;}
                    p->remaining=n;p->state=n?H_CHUNK:H_TRAILER;
                }
                p->llen=0;
            }
        }
    }
    *used=i;return p->state==H_END?N70_DONE:N70_MORE;
}
int n70_http_eof(n70_http*p)
{ if(p->state==H_CLOSE){p->state=H_END;return N70_DONE;}return p->state==H_END?N70_DONE:N70_BAD; }
int n70_http_reusable(const n70_http*p,int clean)
{return clean&&p->state==H_END&&p->self_delimited&&!p->connection_close&&p->status!=408&&(p->version11||p->connection_keepalive);}

static int safe(const char*s)
{ for(;*s;++s)if((unsigned char)*s<=32||(unsigned char)*s>=127||*s=='\\')return 0;return 1; }
int n70_url_parse(const char*s,n70_url*u)
{
    const char *p,*end,*colon=NULL,*tail;size_t n;unsigned port=0;
    if(!safe(s)||strlen(s)>N70_URL_MAX||strlen(s)<8)return 0;
    if(lower(s[0])!='h'||lower(s[1])!='t'||lower(s[2])!='t'||lower(s[3])!='p'||lower(s[4])!='s'||strncmp(s+5,"://",3))return 0;
    p=s+8;end=p;while(*end&&*end!='/'&&*end!='?'&&*end!='#'){if(*end==':')colon=end;++end;}
    n=(size_t)((colon?colon:end)-p);if(!n||n>253)return 0;
    {size_t i;for(i=0;i<n;++i){int c=(unsigned char)p[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'))return 0;u->host[i]=(char)lower(c);}u->host[n]=0;}
    u->port=443;if(colon){for(p=colon+1;p<end;++p){if(*p<'0'||*p>'9')return 0;port=port*10+*p-'0';if(port>65535)return 0;}if(!port)return 0;u->port=port;}
    tail=strchr(end,'#');if(!tail)tail=end+strlen(end);n=(size_t)(tail-end);
    if(n+(end[0]=='/'?0:1)>3072)return 0;
    u->target[0]=0;if(*end!='/')strcpy(u->target,"/");strncat(u->target,end,n);return 1;
}
int n70_url_format(const n70_url*u,char*out,size_t cap)
{
    char port[16];size_t n;
    if(u->port==443)port[0]=0;else sprintf(port,":%u",u->port);
    n=8+strlen(u->host)+strlen(port)+strlen(u->target);
    if(n>=cap)return 0;
    strcpy(out,"https://");strcat(out,u->host);strcat(out,port);strcat(out,u->target);return 1;
}
int n70_same_origin(const n70_url*a,const n70_url*b)
{ return a->port==b->port&&equal(a->host,b->host); }
static void normalize(char*path)
{
    char out[3073],query[3073],*q=strchr(path,'?');size_t i=0,n=0;
    if(q){strcpy(query,q);*q=0;}else query[0]=0;
    /* RFC 3986 remove-dot-segments; preserve repeated and trailing slashes. */
    while(path[i]){
        if(!strncmp(path+i,"/./",3)){i+=2;continue;}
        if(!strcmp(path+i,"/.")){i+=2;out[n++]='/';break;}
        if(!strncmp(path+i,"/../",4)||!strcmp(path+i,"/..")){
            int last=!strcmp(path+i,"/..");i+=3;
            if(n){--n;while(n&&out[n]!='/')--n;}
            if(last){out[n++]='/';break;}continue;
        }
        if(path[i]=='/')out[n++]=path[i++];
        while(path[i]&&path[i]!='/')out[n++]=path[i++];
    }
    if(!n)out[n++]='/';out[n]=0;
    strcpy(path,out);strcat(path,query);
}
int n70_url_resolve(const n70_url*base,const char*location,n70_url*out)
{
    char temp[N70_URL_MAX+1],relative[N70_URL_MAX+1];char*q;size_t n;
    if(!safe(location)||strlen(location)>N70_URL_MAX)return 0;
    strcpy(relative,location);q=strchr(relative,'#');if(q)*q=0;
    {char*colon=strchr(relative,':');char*sep=strpbrk(relative,"/?");
     if(colon&&(!sep||colon<sep))return n70_url_parse(relative,out);}
    if(!strncmp(relative,"//",2)){if(strlen(relative)+6>=sizeof temp)return 0;strcpy(temp,"https:");strcat(temp,relative);return n70_url_parse(temp,out);}
    *out=*base;
    if(!relative[0])return 1;
    if(relative[0]=='?'){q=strchr(out->target,'?');if(q)*q=0;n=strlen(out->target);}
    else if(relative[0]=='/'){out->target[0]=0;n=0;}
    else{q=strchr(out->target,'?');if(q)*q=0;q=strrchr(out->target,'/');if(q)q[1]=0;else strcpy(out->target,"/");n=strlen(out->target);}
    if(n+strlen(relative)>3072)return 0;strcat(out->target,relative);normalize(out->target);return 1;
}
