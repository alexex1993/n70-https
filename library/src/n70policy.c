#include "n70policy.h"
#include <string.h>
#define MAX_MS 2147483647
int n70_method_valid(const char*m)
{return !strcmp(m,"GET")||!strcmp(m,"HEAD")||!strcmp(m,"POST")||!strcmp(m,"PUT")||!strcmp(m,"PATCH")||!strcmp(m,"DELETE");}
int n70_method_has_body(const char*m){return strcmp(m,"GET")&&strcmp(m,"HEAD");}
int n70_method_retryable(const char*m,int idempotent)
{return !strcmp(m,"GET")||!strcmp(m,"HEAD")||(idempotent&&(!strcmp(m,"PUT")||!strcmp(m,"DELETE")));}
int n70_retry_status(int s){return s==408||s==429||s==502||s==503||s==504;}
int n70_transport_error(int e)
{return e==-18||e==-25||e==-29||e==-33||e==-34||e==-36||e==-13||e==-15||e==-8||
    e==-190||e==-191||e==-5100||e==-5104||e==-5105||e==-5106||e==-5109;}
/* Include S60's network/no-route/address-expired/no-DNS errors, not just e32 errors. */
int n70_bearer_error(int e)
{return e==-18||e==-29||e==-36||e==-13||e==-8||e==-190||e==-5104||e==-5105||e==-5106||e==-5109;}
int n70_retry_allowed(const char*m,int idempotent,int delivered,int error,int control)
{return !control&&!delivered&&n70_method_retryable(m,idempotent)&&n70_transport_error(error);}
static int leap(int y){return y%4==0&&(y%100!=0||y%400==0);}
uint64_t n70_epoch_seconds(int y,int mon,int day,int h,int min,int sec)
{
    static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};int d,i,ym1;
    if(y<1970||y>9999||mon<1||mon>12||day<1||day>days[mon-1]+(mon==2&&leap(y))||h<0||h>23||min<0||min>59||sec<0||sec>59)return UINT64_MAX;
    ym1=y-1;d=365*(y-1970)+(ym1/4-ym1/100+ym1/400)-(1969/4-1969/100+1969/400);
    for(i=1;i<mon;++i)d+=days[i-1]+(i==2&&leap(y));d+=day-1;
    return (uint64_t)d*86400+h*3600+min*60+sec;
}
static int number(const char*s,int n)
{int x=0,i;for(i=0;i<n;++i){if(s[i]<'0'||s[i]>'9')return -1;x=x*10+s[i]-'0';}return x;}
static int month(const char*s)
{static const char names[]="JanFebMarAprMayJunJulAugSepOctNovDec";int i;for(i=0;i<12;++i)if(!memcmp(s,names+3*i,3))return i+1;return 0;}
static int weekday(const char*s,size_t n)
{static const char*const w[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat","Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};size_t i;for(i=0;i<14;++i)if(strlen(w[i])==n&&!memcmp(w[i],s,n))return 1;return 0;}
int n70_retry_after(const char*s,size_t n,uint64_t now,int current_year)
{
    size_t i;unsigned v=0;int y=0,m=0,d=0,h=-1,min=-1,sec=-1;uint64_t date,diff;
    if(!n)return -1;
    for(i=0;i<n&&s[i]>='0'&&s[i]<='9';++i){unsigned digit=s[i]-'0';if(v>2147483U||(v==2147483U&&digit>6))v=2147484U;else v=v*10+digit;}
    if(i==n)return v>2147483U?MAX_MS:(int)(v*1000);
    if(n==29&&s[3]==','&&s[4]==' '&&s[7]==' '&&s[11]==' '&&s[16]==' '&&s[19]==':'&&s[22]==':'&&!memcmp(s+25," GMT",4)&&weekday(s,3)){
        d=number(s+5,2);m=month(s+8);y=number(s+12,4);h=number(s+17,2);min=number(s+20,2);sec=number(s+23,2);
    }else if(n==24&&s[3]==' '&&s[7]==' '&&s[10]==' '&&s[13]==':'&&s[16]==':'&&s[19]==' '&&weekday(s,3)){
        m=month(s+4);d=s[8]==' '?number(s+9,1):number(s+8,2);h=number(s+11,2);min=number(s+14,2);sec=number(s+17,2);y=number(s+20,4);
    }else{
        const char*comma=(const char*)memchr(s,',',n);size_t p=comma?(size_t)(comma-s):0;
        if(!comma||!weekday(s,p)||p+24!=n)return -1;s+=p+1;
        if(s[0]!=' '||s[3]!='-'||s[7]!='-'||s[10]!=' '||s[13]!=':'||s[16]!=':'||memcmp(s+19," GMT",4))return -1;
        d=number(s+1,2);m=month(s+4);y=number(s+8,2);h=number(s+11,2);min=number(s+14,2);sec=number(s+17,2);
        if(y<0)return -1;y+=(current_year/100)*100;if(y>current_year+50)y-=100;
    }
    date=n70_epoch_seconds(y,m,d,h,min,sec);if(date==UINT64_MAX)return -1;
    if(date<=now)return 0;diff=date-now;if(diff>2147483U)return MAX_MS;return (int)diff*1000;
}
int n70_retry_delay(int base,int cap,int attempt,int after,unsigned jitter)
{
    int i,delay=base;if(base<0||cap<base||attempt<0||attempt>20)return -1;
    for(i=0;i<attempt&&delay<cap;++i)delay=delay>cap/2?cap:delay*2;
    if(delay>cap)delay=cap;
    if(delay&&delay<cap){unsigned extra=jitter%((unsigned)delay/4+1);delay+=(int)extra;if(delay>cap)delay=cap;}
    if(after>cap)return -1;if(after>delay)delay=after;return delay;
}
