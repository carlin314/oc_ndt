#ifndef MAP2D_H
#define MAP2D_H

#include<iostream>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include<list>
#include "robot.h"
#include"Vec2.h"
#include"Stopwatch.h"
#include<float.h>
#include<map>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <boost/thread/thread.hpp>
#include <pcl/common/common_headers.h>
#include <pcl/features/normal_3d.h>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/console/parse.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>


using namespace Eigen;
using namespace std;

#define MINPOINTSIZE 4

namespace daysun{
//new one - ABCD UD
struct OcNode
{
//    Vec3 min,max;
    std::list<pcl::PointXYZ> lPoints;
    Eigen::Matrix3f covariance_matrix; //C
    Eigen::Vector3f xyz_centroid; //mean
    int N; //number of points which has been droped(mean having been calculated )
    string morton;
    string z;
    OcNode //Constructor
    (string z ="",string morton = "")
        : z(z){
        covariance_matrix = Eigen::Matrix3f::Zero(3,3);
        xyz_centroid = Eigen::Vector3f::Zero();
        N =0;
    }

    bool isEmpty(){
        if((N < MINPOINTSIZE) && (covariance_matrix == Eigen::Matrix3f::Zero(3,3)) && (xyz_centroid == Eigen::Vector3f::Zero()))
            return true;// emtpty
        else return false; //full
    }

    bool isSlope(multimap<string,OcNode *> map_xy,bool & up, bool & down){
        int currentZ = strToInt( z.substr(1,z.length()-1)); // this node
        string belong = z.substr(0,1);
        string zadd = stringAndFloat(belong,(currentZ +1));
        if(currentZ == 1){
            belong.compare("U") == 0? belong="D":belong="U";
            currentZ +=1;
        }
        string zminus = stringAndFloat(belong,(currentZ-1));
//        cout<<"add1,minus1:"<<zadd<<","<<zminus<<endl;

        multimap<string,OcNode *>::iterator it = map_xy.find(morton);
        bool zup=false,zdown=false;
        while(it != map_xy.end()){
            if((it->first).compare(morton) != 0)
                break;
            if(zup == true && zdown == true)
                break;
            if((it->second)->z.compare(zadd) == 0){
                zup = true; //exist
//                if(!(it->second)->isEmpty())
                    up = true;
            }
            if((it->second)->z.compare(zminus) == 0){
                zdown = true; //exist
//                if(!(it->second)->isEmpty())
                    down = true;
            }
            it++;
        }
        if(up==true && down == true)
            return false;
        else return true;
    }

    void countRoughNormal(float & roughness,Vector3f & normalVector){
        EigenSolver<Matrix3f> es(covariance_matrix);
        Matrix3f eigenvalue  = es.pseudoEigenvalueMatrix(); //value
        Matrix3f eigenvector = es.pseudoEigenvectors();  //vector
        if(eigenvalue(0,0) < eigenvalue(1,1)){
            if(eigenvalue(0,0) < eigenvalue(2,2)){
                roughness = eigenvalue(0,0) ;
                normalVector = eigenvector.col(0);
            }else{
                roughness = eigenvalue(2,2) ;
                normalVector = eigenvector.col(2);
            }
        }else{
            if(eigenvalue(1,1) < eigenvalue(2,2)){
                roughness = eigenvalue(1,1) ;
                normalVector = eigenvector.col(1);
            }else{
                roughness = eigenvalue(2,2) ;
                normalVector = eigenvector.col(2);
            }
        }
        if(roughness == 0) //for visualization
            roughness = 0.01;
    }

};

struct CmpByKeyUD {
  bool operator()(const string& k1, const string& k2) {
      int num1 = strToInt( k1.substr(1,k1.length()-1));
      string be1 = k1.substr(0,1);
      int num2 = strToInt( k2.substr(1,k2.length()-1));
      string be2 = k2.substr(0,1);
      be1.compare("D") == 0? num1 *= -1: num1 = num1;
      be2.compare("D") == 0? num2*=-1: num2= num2;
      return num1<num2;
  }
};

struct Slope{
    Vector3f normal; //Normal vector of the slope
    float rough;  //roughness of the slope
     Vec3 mean;  //mean value of the slope
     float h;
     float g;
     float f;  //f = g+h
     string morton_xy,morton_z;
     bool up, down; //false-empty true-full
     Slope * father;
};


class Cell{
    string morton;
public:
    map<string,Slope *,CmpByKeyUD> map_slope;
    Cell(const string morton):morton(morton){}    
    string getMorton(){return morton;}

};


class TwoDmap {
    float gridLen; //resolution      
    Vec3 cloudFirst; //initial--the first node--deem as (0,0,0)

    //adjust the ABCD and count out the left right forward and backward
    void countLRFB(string belongXY,int x,int y,string & leftMtn,string & rightMtn,string & forMtn,string &backMtn){
        int leftx,rightx,forwardx,backx,lefty,righty,forwardy,backy;
        string leftBe,rightBe,forBe,backBe;
        leftBe = rightBe = forBe = backBe = belongXY;
        leftx = x; rightx = x;
        forwardy = y;backy = y;
        if(belongXY.compare("A") == 0){
            forwardx = x+1;backx = x-1;
            lefty = y-1;righty = y+1;
        }
        if(belongXY.compare("B") == 0){
            forwardx = x+1;backx = x-1;
            lefty = y+1;righty = y-1;
        }
        if(belongXY.compare("C") == 0){
            forwardx = x-1;backx = x+1;
            lefty = y-1;righty = y+1;
        }
        if(belongXY.compare("D") == 0){
            forwardx = x-1;backx = x+1;
            lefty = y+1;righty = y-1;
        }
        if(x==1){
            if(belongXY.compare("A") == 0){
                backBe = "C";
                backx = 1;
            }
            if(belongXY.compare("B") == 0){
                backBe = "D";
                backx = 1;
            }
            if(belongXY.compare("C") == 0){
                forBe = "A";
                forwardx = 1;
            }
            if(belongXY.compare("D") == 0){
                forBe = "B";
                forwardx = 1;
            }
        }
        if(y == 1){
            if(belongXY.compare("A") == 0){
                leftBe = "B";
                lefty = 1;
            }
            if(belongXY.compare("C") == 0){
                leftBe = "D";
                lefty = 1;
            }
            if(belongXY.compare("B") == 0){
                rightBe = "A";
                righty = 1;
            }
            if(belongXY.compare("D") == 0){
                rightBe = "C";
                righty = 1;
            }
        }
//        cout<<"count "<<leftBe<<","<<leftx<<","<<lefty<<endl;
//        cout<<"count "<<rightBe<<","<<rightx<<","<<righty<<endl;
//        cout<<"count "<<forBe<<","<<forwardx<<","<<forwardy<<endl;
//        cout<<"count "<<backBe<<","<<backx<<","<<backy<<endl;
        leftMtn = stringAndFloat(leftBe, countMorton(leftx,lefty));
        rightMtn = stringAndFloat(rightBe,countMorton(rightx,righty));
        forMtn = stringAndFloat(forBe,countMorton(forwardx,forwardy));
        backMtn = stringAndFloat(backBe,countMorton(backx,backy));
    }

    //turn morton_z into +-num
    int mtnZToNum(string k1){
        int num1 = strToInt( k1.substr(1,k1.length()-1));
        string be1 = k1.substr(0,1);
        be1.compare("D") == 0? num1 *= -1: num1 = num1;
        return num1;
    }

    void countReachable(string leftMtn, list<Slope *> & list,RobotSphere & robot,
                        string morton_z,Vector3f normal){
        //find the corresponding cell-mortonxy
        map<string,Cell *>::iterator mit=  map_cell.find(leftMtn);
        if(mit != map_cell.end()){
            map<string,Slope *,CmpByKeyUD>::iterator sit =   (mit->second)->map_slope.begin();
            while(sit != (mit->second)->map_slope.end()){
                //judge if it's traversible -morton_z normal rough
                  if(sit->second->up != true)
                if((sit->second)->rough <= robot.getRough())
                    if(countAngle((sit->second)->normal,normal) <= robot.getAngle())
                        if((abs(mtnZToNum((sit->second)->morton_z) - mtnZToNum(morton_z)) <= ceil((float)robot.getReachableHeight()/gridLen)))
                            list.push_back(sit->second);
                  sit++;
                }
            }
    }   

    //true-collide, false-no collide
    bool CollisionCheck(Slope * slope,float r){
        if(slope->up == true)
            return true; //collide
        string xy = slope->morton_xy;
        string z= slope->morton_z;
        map<string,Cell *>::iterator it = map_cell.find(xy);
        if(it != map_cell.end()){
                map<string,Slope *,CmpByKeyUD>::iterator ss = (it->second)->map_slope.find(z); //Ascending order
                if(ss == (it->second)->map_slope.end()) cout<<"collide wrong\n";
                ss++; //the next one
                if(ss != (it->second)->map_slope.end()){
                    int height = mtnZToNum((ss->second)->morton_z);
                    if(height < (mtnZToNum(z)+2*r/gridLen)){
                        return true;//collide
                    }else{
                        return false;//no collide
                    }
                }
                return false;//no collide
        }
    }

    float countAngle(Vector3f n1, Vector3f n2){
        float res = n1.dot(n2) / (sqrt(pow(n1[0],2) + pow(n1[1],2)+pow(n1[2],2))* sqrt(pow(n2[0],2) + pow(n2[1],2)+pow(n2[2],2)));
        float an = acos(res)*180/M_PI;
        an>90? an = 180-an:an=an;
        return  an;
    }

    bool isContainedQ(Slope * s,  list<Slope *> & Q){
        list<Slope *>::iterator it = Q.begin();
        while(it != Q.end()){
            if(s->morton_xy.compare((*it)->morton_xy )==0 && s->morton_z.compare((*it)->morton_z )==0 )
                return true;
            it++;
        }
        return false;
    }
public:    
    multimap<string,OcNode *>  map_xy,map_z; //ABCD+morton, UD+height---index
    TwoDmap(const float res):gridLen(res){}
    float getGridLen(){return gridLen;}
    void setCloudFirst(Vec3 p){
        cloudFirst = p;
    }

    list<string> morton_list; //xy-morton-all
    list<string> changeMorton_list; //temp-change
    map<string,Cell *> map_cell; //xy_morton, cell

    //find slope based on position
  Slope *  findSlope(Vec3 pos, string & morton_xy,string & morton_z){
         transMortonXYZ(pos,morton_xy,morton_z);
         map<string,Cell *>::iterator it = map_cell.find(morton_xy);
         Slope * p = NULL;
         if(it != map_cell.end()){
             map<string,Slope *,CmpByKeyUD>::iterator ss = (it->second)->map_slope.find(morton_z);
             if(ss != (it->second)->map_slope.end()){
                 return ss->second;
             }
         }
    }

  //for now-- no consideration for the height of destination
  float TravelCost(Vec3 cur,Vec3 des,float goal = 0){
      float cost = sqrt(pow(cur.x-des.x,2) + pow(cur.y-des.y,2));
//        if(goal != 0){
//            if(goal > cur.z){
//                des.z > cur.z ? cost=cost: cost += (cur.z-des.z)*0.3 ; //parameters should be changed
//            }else{
//                 des.z < cur.z ? cost=cost: cost += (des.z - cur.z)*0.3 ;
//            }
//        }
        return cost;
  }

    //find the reachable surrounding slopes
     list<Slope *>  AccessibleNeighbors(Slope * slope,RobotSphere & robot){
//         cout<<"access slope: "<<slope->morton_xy<<","<<slope->morton_z<<endl;
         list<Slope *> list;
         string morton_xy = slope->morton_xy;
         string morton_z= slope->morton_z;
         Vector3f normal = slope->normal;
         int x,y;
         string belongXY = morton_xy.substr(0,1);
         int morton = strToInt( morton_xy.substr(1,morton_xy.length()-1));
         mortonToXY(x,y,morton);
         string leftMtn,rightMtn,forMtn,backMtn;
         countLRFB(belongXY,x,y,leftMtn,rightMtn,forMtn,backMtn); //adjust belong
         countReachable(leftMtn,list,robot,morton_z,normal);
         countReachable(rightMtn,list,robot,morton_z,normal);
         countReachable(forMtn,list,robot,morton_z,normal);
         countReachable(backMtn,list,robot,morton_z,normal);
         return list;
     }

    //inital    
    bool create2DMap(){
//        cout<<"start create 2D map\n";
        //get all the mortons-new cell, map.push_back
        list<string>::iterator itor = morton_list.begin();
            while(itor!=morton_list.end())
            {
                Cell * cell = new Cell(*itor);
                map_cell.insert(map<string,Cell*>::value_type(cell->getMorton(), cell));
                //for each morton
                //---find the nodes, count the u,c, drop the points inside
                //---determine which nodes has to be stored in the map
                if(map_xy.count(*itor) == 0){
                    cout<<"wrong\n";
                    return false;
                }else{
                    multimap<string,daysun::OcNode *>::iterator  it = map_xy.find(*itor);
                    while(it != map_xy.end()){
                        if((it->first).compare(*itor) != 0)
                            break;
                        if((it->second)->lPoints.size() >= MINPOINTSIZE){
                            pcl::PointCloud<pcl::PointXYZ>::Ptr point_cloud_ptr (new pcl::PointCloud<pcl::PointXYZ>);
                            std::list<pcl::PointXYZ>::iterator node_iter = (it->second)->lPoints.begin();
                            while(node_iter != (it->second)->lPoints.end()){
                                 point_cloud_ptr->points.push_back (*node_iter);
                                 node_iter++;
                            }
                            //compute mean and covariance_matrix, then drop the pcl
                            Eigen::Matrix3f covariance_matrix; //C
                            Eigen::Vector4f xyz_centroid; //mean
                            pcl::compute3DCentroid(*point_cloud_ptr,xyz_centroid);
                            pcl::computeCovarianceMatrix(*point_cloud_ptr,xyz_centroid,covariance_matrix);
                            (it->second)->xyz_centroid <<xyz_centroid(0),xyz_centroid(1),xyz_centroid(2);
                            (it->second)->covariance_matrix = covariance_matrix;
                              (it->second)->N += (it->second)->lPoints.size(); //record the num counting mean and C                         
                            (it->second)->lPoints.clear();
                            // if this node's up-down neighbors are free
                            //--- store in the slope  (count roughness and Normal vector, (new slope, cell.push_back)
                            // if not free, ignore them
                            bool up= false, down = false;
                            if((it->second)->isSlope(map_xy,up,down) ){
                                Slope * slope = new Slope();
                                cell->map_slope.insert(make_pair((it->second)->z,slope));
                                slope->mean.x = xyz_centroid(0);
                                slope->mean.y = xyz_centroid(1);
                                slope->mean.z = xyz_centroid(2);
                                slope->morton_xy = (it->second)->morton;
                                slope->morton_z= (it->second)->z;
                                slope->up = up,slope->down = down;
                                slope->h = slope->g = slope->f = FLT_MAX;
                                slope->father = NULL;
                                (it->second)->countRoughNormal(slope->rough,slope->normal);
                            }
                        }
                        it++;
                    }
                }
                itor++;
            }        
        return true;
    }

    //for visualization
    void countPositionXYZ(float & x,float &y,float &z,string s_xy,string s_z){
        string belongxy = s_xy.substr(0,1);
        string belongz = s_z.substr(0,1);
         int mortonxy = strToInt( s_xy.substr(1,s_xy.length()-1));
         int mortonz = strToInt( s_z.substr(1,s_z.length()-1));
         int a,b;
         mortonToXY(a,b,mortonxy);
         if(belongxy.compare("A") == 0){
             x = (a-0.5)*gridLen + cloudFirst.x;
             y = (b-0.5)*gridLen + cloudFirst.y;
         }
         if(belongxy.compare("B") == 0){
             x = (a-0.5)*gridLen + cloudFirst.x;
             y = cloudFirst.y - (b-0.5)*gridLen ;
         }
         if(belongxy.compare("C") == 0){
             x = cloudFirst.x- (a-0.5)*gridLen ;
             y = (b-0.5)*gridLen + cloudFirst.y;
         }
         if(belongxy.compare("D") == 0){
             x = cloudFirst.x- (a-0.5)*gridLen ;
             y = cloudFirst.y - (b-0.5)*gridLen ;
         }
         if(belongz.compare("U") == 0){
             z = cloudFirst.z + (mortonz-0.5)*gridLen;
         }
         else if(belongz.compare("D") == 0){
             z = cloudFirst.z - (mortonz-0.5)*gridLen;
         }
    }

    //for visualiation
    void showSlopeList(ros::Publisher marker_pub,list<Slope *> & closed,float radius,int color =0){
        ros::Rate r(50);
        uint32_t shape = visualization_msgs::Marker::CUBE; //SPHERE ARROW CYLINDER
        visualization_msgs::MarkerArray mArray;
        int i = 0;
//        cout<<closed.size()<<endl;
        list<Slope *>::iterator it = closed.begin();
        while(it != closed.end()){
            Vector3f normal = (*it)->normal;
//            Vec3 mean = (*it)->mean;
            float rough = (*it)->rough;
            float x,y,z;
            string s_xy = (*it)->morton_xy;
            string s_z = (*it)->morton_z;
            countPositionXYZ(x,y,z,s_xy,s_z);
                    visualization_msgs::Marker m_s;
                    m_s.ns  = "traversibility";
                    m_s.header.frame_id = "/my_frame";
                    m_s.header.stamp = ros::Time::now();
                    m_s.id = i;
                    m_s.type = shape;
                    m_s.action = visualization_msgs::Marker::ADD;
                    m_s.pose.position.x = x;
                    m_s.pose.position.y = y;
                    m_s.pose.position.z = z;
                    m_s.pose.orientation.x = normal(0);
                    m_s.pose.orientation.y = normal(1);
                    m_s.pose.orientation.z = normal(2);
                    m_s.pose.orientation.w = 1.0;
                    m_s.scale.x = radius;
                    m_s.scale.y = radius;
                    m_s.scale.z = 2.5* rough; //not using the rough, for visualization
                    if(color == 0){ //traversibility
                        m_s.color.a = 1.0;
                        m_s.color.b = 1.0;
                        m_s.color.r = 0.5;
                    }
                    else if(color == 1){ //not traversible
                        m_s.color.a = 1.0;
                        m_s.color.b = 0.5;
                        m_s.color.r = 1;
                    }
                    else if(color == 2){ //for bottom_cell
                        m_s.color.a = 1.0;
                        m_s.color.r = 0.5;
                        m_s.color.g = 0.2;
                    }else if(color == 4){ //for route
                        m_s.color.a = 1.0;
                        m_s.color.b= 0.5;
                        m_s.color.r = 0.5;
                        m_s.color.g = 0.5;
                        m_s.scale.z = radius;
                    }
                    m_s.lifetime = ros::Duration();
                    mArray.markers.push_back(m_s);
            it++;i++;
            }
        if(ros::ok()){
            if (marker_pub.getNumSubscribers() == 1){
                marker_pub.publish(mArray);
                ros::spinOnce();
                   r.sleep();
            }
        }
    }

    //for visualization-initial
    void showInital(ros::Publisher marker_pub,RobotSphere & robot,int color =0){ //0-,1-change
        float radius=robot.getR();
        ros::Rate r(50);
        uint32_t shape = visualization_msgs::Marker::CUBE; //SPHERE ARROW CYLINDER
         visualization_msgs::MarkerArray mArray;
        int i = 2;
        if (ros::ok()){
            //1-for start and goal
            {
            list<Vec3> lv ;
            lv.push_back(robot.getGoal());
            lv.push_back(robot.getPosition());
            list<Vec3>::iterator ilv = lv.begin();
            int j = 0;
            while(ilv != lv.end()){
                string m_xy,m_z;
                float x,y,z;
                transMortonXYZ(*ilv,m_xy,m_z);
                countPositionXYZ(x,y,z,m_xy,m_z);
                visualization_msgs::Marker m_s;
                m_s.ns  = "basic_shapes";
                m_s.header.frame_id = "/my_frame";
                m_s.header.stamp = ros::Time::now();
                m_s.id = j;
                m_s.type = shape;
                m_s.action = visualization_msgs::Marker::ADD;
                m_s.pose.position.x = x;
                m_s.pose.position.y = y;
                m_s.pose.position.z = z;
                m_s.scale.x = radius; //the same as radius
                m_s.scale.y = radius;
                m_s.scale.z = radius;
                m_s.pose.orientation.x = 0;
                m_s.pose.orientation.y = 0;
                m_s.pose.orientation.z = 0;
                m_s.pose.orientation.w = 1.0;
                m_s.color.a = 1.0;
                m_s.color.r = 0.5;
                m_s.color.g = 0.5;
                m_s.lifetime = ros::Duration();
                mArray.markers.push_back(m_s);
                ilv++;j++;
            }
            }
            //2-for every cell
                    if(map_cell.size() != 0){
                    map<string,Cell*>::iterator cell_iter= map_cell.begin();
                    while(cell_iter != map_cell.end()){                       
                         Cell * cell = cell_iter->second;
                        //for every slope
                         map<string,Slope *,CmpByKeyUD>::iterator slItor = cell->map_slope.begin();                        
                         while(slItor != cell->map_slope.end()){
                             i++;
                            Vector3f normal = (slItor->second)->normal;                          
                            float rough = (slItor->second)->rough;
                            float x,y,z;
                            string s_xy = (slItor->second)->morton_xy;
                            string s_z = (slItor->second)->morton_z;                      
                            countPositionXYZ(x,y,z,s_xy,s_z);                           
                            //add marker
                            visualization_msgs::Marker marker;
                            marker.ns = "basic_shapes";
                            marker.header.frame_id = "/my_frame";
                            marker.header.stamp = ros::Time::now();
                            marker.id = i; //same namespace and id will overwrite the old one
                            marker.type = shape;
                            marker.action = visualization_msgs::Marker::ADD;
                            marker.pose.position.x = x;
                            marker.pose.position.y = y;
                            marker.pose.position.z = z;
                            marker.pose.orientation.x = normal(0);
                            marker.pose.orientation.y = normal(1);
                            marker.pose.orientation.z = normal(2);
                            marker.pose.orientation.w = 1.0;
                            marker.scale.x = radius; //the same as radius
                            marker.scale.y = radius;
                            marker.scale.z = 0.05/*rough*/;
                            marker.color.a = 1.0;
                            marker.color.r = 0.5;                            
                            if(color == 1){
                                //for testing change
                                marker.color.g =0.5;
                                marker.color.b =1;
                            }
                            marker.lifetime = ros::Duration();                            
                            mArray.markers.push_back(marker);
                             slItor++;
                         }
                        cell_iter++;                        
                    }
          }
                    if (marker_pub.getNumSubscribers() == 1){
                        marker_pub.publish(mArray);
                        ros::spinOnce();
                           r.sleep();
                    }
    }
    }

    //for visualization
    void showBottom(ros::Publisher marker_pub,float radius){
        ros::Rate r(50);
        uint32_t shape = visualization_msgs::Marker::CUBE; //SPHERE ARROW CYLINDER
        visualization_msgs::MarkerArray mArray;
        int i =0;
        map<string,Cell *>::iterator it = map_cell.begin();
        while(it != map_cell.end()){
            Cell * cell = it->second;
            Vector3f normal ;
            normal<<0,0,1;
            float rough = 0.1;
            float x,y,z;
            string s_xy = cell->getMorton();
            string s_z = "U1";
            countPositionXYZ(x,y,z,s_xy,s_z);
                    visualization_msgs::Marker m_s;
                    m_s.ns  = "bottom";
                    m_s.header.frame_id = "/my_frame";
                    m_s.header.stamp = ros::Time::now();
                    m_s.id = i;
                    m_s.type = shape;
                    m_s.action = visualization_msgs::Marker::ADD;
                    m_s.pose.position.x = x;
                    m_s.pose.position.y = y;
                    m_s.pose.position.z = z;
                    m_s.pose.orientation.x = normal(0);
                    m_s.pose.orientation.y = normal(1);
                    m_s.pose.orientation.z = normal(2);
                    m_s.pose.orientation.w = 1.0;
                    m_s.scale.x = radius;
                    m_s.scale.y = radius;
                    m_s.scale.z = 0.2/*1.5* rough*/; //not using the rough, for visualization
                        m_s.color.a = 1.0;
                        m_s.color.r = 0.5;
                        m_s.color.g = 0.2;
                    m_s.lifetime = ros::Duration();
                    mArray.markers.push_back(m_s);
            it++; i++;
        }
        if(ros::ok()){
            if (marker_pub.getNumSubscribers() == 1){
                marker_pub.publish(mArray);
                ros::spinOnce();
                   r.sleep();
            }
        }
    }

    //tranform position into morton_xy and morton_z
    void transMortonXYZ(Vec3 position, string & morton_xy, string & morton_z){
        string xy_belong,z_belong;
        if(position.x > cloudFirst.x){
            if(position.y >cloudFirst.y)
                xy_belong = "A";
            else
                xy_belong = "B";
        }else{
            if(position.y >cloudFirst.y)
                xy_belong = "C";
            else
                xy_belong = "D";
        }
        if(position.z > cloudFirst.z) z_belong = "U"; //up
        else z_belong = "D"; //down
        int nx = (int)ceil(float(abs(position.x-cloudFirst.x)/gridLen));
        int ny = (int)ceil(float(abs(position.y-cloudFirst.y)/gridLen));
        int nz = (int)ceil(float(abs(position.z-cloudFirst.z)/gridLen));
        nx == 0? nx =1:nx=nx;
        ny == 0? ny =1:ny=ny;
        nz == 0? nz =1:nz=nz;
        int morton = countMorton(nx,ny);
        morton_xy = stringAndFloat( xy_belong, morton);
        morton_z =stringAndFloat( z_belong , nz);
    }

    /// might have some mistakes
    bool change2DMap(){
        cout<<"start change 2D map\n";
        if(changeMorton_list.size() == 0)
            return false;
        list<string>::iterator itor = changeMorton_list.begin();
        while(itor != changeMorton_list.end()){
            //find if it's contained in cell_morton_list
            if(map_cell.size() == 0)
                return false;
            string changeMorton = *itor;
            map<string,Cell *>::iterator map_it= map_cell.find(changeMorton);
            if(map_it == map_cell.end()) {
                //not find 1
                //create a new cell- the same as the initial
                Cell * cell = new Cell(*itor);
                map_cell.insert(map<string,Cell*>::value_type(cell->getMorton(), cell));
                if(map_xy.count(*itor) == 0){
                    cout<<"wrong\n";
                    return false;
                }else{
                    multimap<string,daysun::OcNode *>::iterator  it = map_xy.find(*itor);
                    while(it != map_xy.end()){
                        if((it->first).compare(*itor) != 0)
                            break;
                        if((it->second)->lPoints.size() >= 3){
                            pcl::PointCloud<pcl::PointXYZ>::Ptr point_cloud_ptr (new pcl::PointCloud<pcl::PointXYZ>);
                            std::list<pcl::PointXYZ>::iterator node_iter = (it->second)->lPoints.begin();
                            while(node_iter != (it->second)->lPoints.end()){
                                 point_cloud_ptr->points.push_back (*node_iter);
                                 node_iter++;
                            }
                            Eigen::Matrix3f covariance_matrix; //C
                            Eigen::Vector4f xyz_centroid; //mean
                            pcl::compute3DCentroid(*point_cloud_ptr,xyz_centroid);
                            pcl::computeCovarianceMatrix(*point_cloud_ptr,xyz_centroid,covariance_matrix);
                            (it->second)->xyz_centroid << xyz_centroid(0),xyz_centroid(1),xyz_centroid(2);
                            (it->second)->covariance_matrix = covariance_matrix;
                              (it->second)->N += (it->second)->lPoints.size();
                            (it->second)->lPoints.clear();
                            bool up= false, down = false;
                            if((it->second)->isSlope(map_xy,up,down) ){
                                Slope * slope = new Slope();
                                cell->map_slope.insert(make_pair((it->second)->z,slope));
                                slope->mean.x = xyz_centroid(0);
                                slope->mean.y = xyz_centroid(1);
                                slope->mean.z = xyz_centroid(2);
                                slope->morton_xy = (it->second)->morton;
                                slope->morton_z= (it->second)->z;
                                slope->up = up,slope->down = down;
//                                 slope->father = NULL;
//                                slope->h = slope->g = slope->f = FLT_MAX;
                                (it->second)->countRoughNormal(slope->rough,slope->normal);
                            }
                        }
                        it++;
                    }
                }
            }else{
                //find 1-handle the exsit cell-update it
                Cell * cell = map_it->second;
                multimap<string,daysun::OcNode *>::iterator  it = map_xy.find(changeMorton);
                while(it != map_xy.end()){
                    if((it->first).compare(changeMorton) != 0)
                        break;
                    if((it->second)->lPoints.size() >= 3){
                        pcl::PointCloud<pcl::PointXYZ>::Ptr point_cloud_ptr (new pcl::PointCloud<pcl::PointXYZ>);
                        std::list<pcl::PointXYZ>::iterator node_iter = (it->second)->lPoints.begin();
                        while(node_iter != (it->second)->lPoints.end()){
                             point_cloud_ptr->points.push_back (*node_iter);
                             node_iter++;
                        }
                        if((Eigen::Matrix3f::Zero(3,3) == (it->second)->covariance_matrix )
                                && (Eigen::Vector3f::Zero()==  (it->second)->xyz_centroid)){
                            //not initial
                            Eigen::Matrix3f covariance_matrix; //C
                            Eigen::Vector4f xyz_centroid; //mean
                            pcl::compute3DCentroid(*point_cloud_ptr,xyz_centroid);
                            pcl::computeCovarianceMatrix(*point_cloud_ptr,xyz_centroid,covariance_matrix);
                            (it->second)->xyz_centroid <<xyz_centroid(0),xyz_centroid(1),xyz_centroid(2);
                            (it->second)->covariance_matrix = covariance_matrix;
                        }else{
                            //has initial data--update
                             Eigen::Matrix3f C0 = (it->second)->covariance_matrix;
                             Eigen::Vector3f u0 = (it->second)->xyz_centroid;
                             int N =  (it->second)->N ;
                             int M =  (it->second)->lPoints.size();
                             Eigen::Matrix3f C1,covariance_matrix;
                             Eigen::Vector4f temp_u;
                             Eigen::Vector3f u1,xyz_centroid;
                             pcl::compute3DCentroid(*point_cloud_ptr,temp_u);
                             pcl::computeCovarianceMatrix(*point_cloud_ptr,temp_u,C1);
                             u1<<temp_u(0),temp_u(1),temp_u(2);
                             xyz_centroid =
                                     ( N*u0 + M*u1) / (M+N);
                             covariance_matrix =
                                     ((N-1)*C0 + (M-1)*C1 + M*N/(M+N)*((u0-u1)*((u0-u1).transpose()))) / (M+N-1);
                             (it->second)->xyz_centroid = xyz_centroid;
                             (it->second)->covariance_matrix = covariance_matrix;
//                                      cout<<"has initial data--update\n"<<  (it->second)->xyz_centroid
//                                         <<endl<<(it->second)->covariance_matrix <<endl;
                        }
                        (it->second)->N += (it->second)->lPoints.size();
                        (it->second)->lPoints.clear();
                        bool up= false, down = false;
                        if((it->second)->isSlope(map_xy,up,down) ){
                            Slope * slope = new Slope();
                            cell->map_slope.insert(make_pair((it->second)->z,slope));
                            slope->mean.x = (it->second)->xyz_centroid(0);
                            slope->mean.y = (it->second)->xyz_centroid(1);
                            slope->mean.z = (it->second)->xyz_centroid(2);
                            slope->morton_xy = (it->second)->morton;
                            slope->morton_z= (it->second)->z;
                            slope->up = up,slope->down = down;
//                             slope->father = NULL;
//                            slope->h = slope->g = slope->f = FLT_MAX;
                            (it->second)->countRoughNormal(slope->rough,slope->normal);
                        }
                    }
                    it++;
                }
            }
             itor++;
        } //change morton list end
            cout<<"change 2D map done.\n";
            return true;
    }

    //compute cost map
    void computeCost(Vec3 goal,RobotSphere & robot,ros::Publisher marker_pub){
        double time_start2 = stopwatch();
         list<Slope *> Q; //list of cell to be computed
         list<Slope *> closed;//no checking again
         list<Slope *> traversability; //can travel
         string morton_xy,morton_z;
         transMortonXYZ(goal,morton_xy,morton_z);
         map<string,Cell *>::iterator it = map_cell.find(morton_xy);
         if(it != map_cell.end()){
             map<string,Slope *,CmpByKeyUD>::iterator ss = (it->second)->map_slope.find(morton_z);
             if(ss != (it->second)->map_slope.end()){
                 ss->second->h = 0; //goal.h = 0
                 Q.push_back( ss->second);
//                 cout<<"find start\n";
             }else{
                 cout<<"Goal position wrong: cant find goal slope.\n";
                 return ;
             }
         }

         //compute the surrounding morton code
         while(Q.size() != 0){
             if(!CollisionCheck(Q.front(),robot.getRobotR() )){              
                 //no collision
                 list<Slope *> neiSlope = AccessibleNeighbors(Q.front(),robot);
                 list<Slope *>::iterator itN = neiSlope.begin();
                 while(itN != neiSlope.end()){
                     if((*itN)->up == true){
                         (*itN)->h = FLT_MAX;
                         closed.push_back(*itN);
                     }                     
                     else{
                         Vec3 q,itn;
                         string s_xy = Q.front()->morton_xy;
                         string s_z = Q.front()->morton_z;
                         countPositionXYZ(q.x,q.y,q.z,s_xy,s_z);
                         string n_xy = (*itN)->morton_xy;
                         string n_z = (*itN)->morton_z;
                         countPositionXYZ(itn.x,itn.y,itn.z,n_xy,n_z);

                         if((*itN)->h > Q.front()->h + TravelCost(q,itn,goal.z)){
                         (*itN)->h = Q.front()->h + TravelCost(q,itn,goal.z);
                         if(!isContainedQ(*itN,Q) && !isContainedQ(*itN,closed) && !isContainedQ(*itN,traversability)){
                             Q.push_back(*itN);
                         }
                     }
                     }
                     itN++;
                 }
                 traversability.push_back(Q.front());
             }else{
                 Q.front()->h = FLT_MAX; //collide
                 closed.push_back(Q.front());
             }
             Q.pop_front();
         }
         double time_end2 = stopwatch();
         cout<<"Compute costmap done. Time cost: "<<(time_end2-time_start2)<<" s\n";
         if(marker_pub.getNumSubscribers()){
             showSlopeList(marker_pub,traversability,robot.getR(),0);
//             showSlopeList(marker_pub,closed,robot.getR(),1); //for test
             cout<<"costmap show done\n";
         }
    }

};

}
#endif // MAP2D_H
