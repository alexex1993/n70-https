#include "n70headers.h"
#include "n70http.h"
#include <string.h>
static int lower(int c){return c>='A'&&c<='Z'?c+32:c;}
static int same(const char*a,size_t n,const char*b,size_t m)
{size_t i;if(n!=m)return 0;for(i=0;i<n;++i)if(lower((unsigned char)a[i])!=lower((unsigned char)b[i]))return 0;return 1;}
static int token(int c)
{return c&&((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||strchr("!#$%&'*+-.^_`|~",c)!=NULL);}
static int find(const char*d,size_t n,const char*key,size_t k,size_t*start,size_t*end)
{
    size_t p=0,c,e;
    while(p<n){c=p;while(c<n&&d[c]!=':')++c;e=c;
        while(e+1<n&&(d[e]!='\r'||d[e+1]!='\n'))++e;
        if(c==n||e+1>=n)return 0;
        if(same(d+p,c-p,key,k)){*start=p;*end=e+2;return 1;}p=e+2;
    }return 0;
}
int n70_headers_has(const char*d,size_t n,const char*k,size_t m)
{size_t a,b;return find(d,n,k,m,&a,&b);}
int n70_headers_set(char*d,size_t*n,size_t cap,const char*key,size_t k,const char*v,size_t m)
{
    static const char*const reserved[]={"Host","Content-Length","Content-Type","Transfer-Encoding",
        "Connection","Accept-Encoding","TE","Trailer","Upgrade","Proxy-Authorization","Proxy-Connection","Expect","Keep-Alive"};
    size_t i,a=0,b=0,old=0,add;
    if(*n>cap||!k||k>128||m>2048)return N70_BAD;
    for(i=0;i<k;++i)if(!token((unsigned char)key[i]))return N70_BAD;
    for(i=0;i<m;++i)if(((unsigned char)v[i]<32&&v[i]!='\t')||(unsigned char)v[i]>=127)return N70_BAD;
    for(i=0;i<sizeof reserved/sizeof reserved[0];++i)if(same(key,k,reserved[i],strlen(reserved[i])))return N70_BAD;
    if(find(d,*n,key,k,&a,&b))old=b-a;
    add=k+2+m+2;if(*n>cap||add>cap-(*n-old))return N70_LIMIT;
    if(old){memmove(d+a,d+b,*n-b);*n-=old;}
    memcpy(d+*n,key,k);*n+=k;memcpy(d+*n,": ",2);*n+=2;
    memcpy(d+*n,v,m);*n+=m;memcpy(d+*n,"\r\n",2);*n+=2;return 0;
}
int n70_bearer_valid(const char*s,size_t n)
{
    size_t i;int padding=0;if(!n||n>2041)return 0;
    for(i=0;i<n;++i){int c=(unsigned char)s[i];if(c=='='){if(!i)return 0;padding=1;continue;}
        if(!c||padding||!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||strchr("-._~+/",c)))return 0;
    }return 1;
}
int n70_headers_forward(int allowed,int same_origin){return allowed&&same_origin;}
