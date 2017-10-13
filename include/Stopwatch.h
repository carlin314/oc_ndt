#ifndef _WIN32
#include <sys/time.h>
#include<iostream>
#include<string>
#include <sstream>
using namespace std;

double stopwatch()
{
	struct timeval time;
	gettimeofday(&time, 0 );
	return 1.0 * time.tv_sec + time.tv_usec / (double)1e6;
}

int decToBinary(int k){
    int s[10],rem,i=0,t=0;
    do{
        rem = k%2;
        k= k/2;
        s[i++] = rem;
    }while(k!=0);
    for(int j = --i;j>=0;j--){
        t += s[j] * pow(10.0,j);
    }
    return t;
}

string intToString(int k){
    stringstream ss;
   ss<<k;
   string str = ss.str();
   return str;
}

int binToDec(int k){
    stringstream ss;
   ss<<k;
   string str = ss.str();
    unsigned int i = 0;
       const char *pch = str.c_str();
       while (*pch == '0' || *pch == '1') {
           i <<= 1;
           i |= *pch++ - '0';
       }
       return (int)i;
}

int binToDec(string str){
    unsigned int i = 0;
       const char *pch = str.c_str();
       while (*pch == '0' || *pch == '1') {
           i <<= 1;
           i |= *pch++ - '0';
       }
       return (int)i;
}

//for example
//line 5: 0101 column 7:0111
//morton:        110111
//return dec:   55
int countMorton(int a,int b){
    string line = intToString(decToBinary(a));
    string column = intToString(decToBinary(b));
    if( line.size()>column.size()){
        int num = line.size()-column.size();
        for(int i =0;i<num;i++){
            column.insert(0,"0");
        }
    }else if(line.size()<column.size()){
        int num = column.size()-line.size();
        for(int i =0;i<num;i++){
            line.insert(0,"0");
        }
    }
 //  cout<<line<<","<<column<<endl;
    int size = line.size() + column.size();
    char * reverse = new char[size+1];
    int j  =0;
    for(int i = line.size()-1;i>=0;i--){
        reverse[j++] = column[i];
        reverse[j++] = line[i];
    }
    reverse[size] = '\0';
    // cout<<reverse<<endl;
    stringstream ss;
   ss<<reverse;
   string str = ss.str();
    string result(str.rbegin(),str.rend());
// cout<<result<<endl;
    int res = binToDec(result);
  //   cout<<res<<endl;
    return  res;
}

int countXYZ(int m,int z){
    string mm = intToString(m);
    string zz = intToString(z);
    string result = mm+zz;
    stringstream ss;
    ss<<result;
    int res;
    ss>>res;
    return res;
}

#else

#include <windows.h>
double stopwatch() 
{
	unsigned long long ticks;
	unsigned long long ticks_per_sec;
	QueryPerformanceFrequency( (LARGE_INTEGER *)&ticks_per_sec);
	QueryPerformanceCounter((LARGE_INTEGER *)&ticks);
	return ((float)ticks) / (float)ticks_per_sec;
}

#endif
