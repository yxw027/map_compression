#include "ros/ros.h"
#include "ros/console.h"

#include "pointmatcher/PointMatcher.h"
#include "pointmatcher/Timer.h"
#include "pointmatcher_ros/point_cloud.h"
#include "pointmatcher_ros/transform.h"
#include "nabo/nabo.h"
#include "eigen_conversions/eigen_msg.h"
#include "pointmatcher_ros/get_params_from_server.h"

#include <tf/transform_broadcaster.h>

#include <fstream>
#include <vector>
#include <algorithm>

#include <visualization_msgs/Marker.h>
#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/Path.h>

#include <numeric>

using namespace std;
using namespace PointMatcherSupport;

class mapCheck
{
    typedef PointMatcher<float> PM;
    typedef PM::DataPoints DP;
    typedef PM::Matches Matches;

    typedef typename Nabo::NearestNeighbourSearch<float> NNS;
    typedef typename NNS::SearchType NNSearchType;

public:
    mapCheck(ros::NodeHandle &n);
    ~mapCheck();
    ros::NodeHandle& n;

    string loadMapName;
    string loadTrajName;
    ros::Publisher mapCloudPub;
    ros::Publisher staticCloudPub;
    ros::Publisher mapPathPub;

    DP mapCloud;
    DP staticCloud;
    int staticInt;
    DP partCloud;

    int mapLength;
    nav_msgs::Path mapPath;

    void wait();
    void process();

};

mapCheck::~mapCheck()
{}

mapCheck::mapCheck(ros::NodeHandle& n):
    n(n),
    loadMapName(getParam<string>("loadMapName", ".")),
    staticInt(getParam<int>("staticInt", 0)),
    loadTrajName(getParam<string>("loadTrajName", ".")),
    mapLength(getParam<int>("mapLength", 0))
{
    mapCloudPub = n.advertise<sensor_msgs::PointCloud2>("mapCloud", 2, true);
    staticCloudPub = n.advertise<sensor_msgs::PointCloud2>("staticCloud", 2, true);
    mapPathPub = n.advertise<nav_msgs::Path>("mapPath", 20000, true);

    // load
    mapCloud = DP::load(loadMapName);

    // read initial transformation
    int x, y;
    double mapPoses [mapLength][16];
    mapPath.header.frame_id = "global";
    mapPath.header.stamp = ros::Time::now();
    ifstream mapin(loadTrajName);
    for (y = 0; y < mapLength; y++)
    {
        for (x = 0; x < 16; x++)
        {
            mapin >> mapPoses[y][x];
        }

        geometry_msgs::PoseStamped pose;
        pose.header.frame_id = mapPath.header.frame_id;
        pose.header.stamp = mapPath.header.stamp;
        pose.pose.position.x = mapPoses[y][3];
        pose.pose.position.y = mapPoses[y][7];
        pose.pose.position.z = mapPoses[y][11];
        pose.pose.orientation = tf::createQuaternionMsgFromYaw(0);  // always 0;

        mapPath.poses.push_back(pose);
    }
    mapin.close();

    cout<<"JUST SHOW"<<endl;

    // process
    this->process();

    staticCloudPub.publish(PointMatcher_ros::pointMatcherCloudToRosMsg<float>(staticCloud, "global", ros::Time(0)));
    mapCloudPub.publish(PointMatcher_ros::pointMatcherCloudToRosMsg<float>(mapCloud, "global", ros::Time(0)));
    mapPathPub.publish(mapPath);

    this->wait();
}

void mapCheck::wait()
{
    ros::Rate r(1);

    while(ros::ok())
    {
    }
}

void mapCheck::process()
{
    /**  CHECK TEST  **/

    // find the max count
    /*
    int max_session = 0;
    int min_session = 21;

    for(int m=0; m<mapCloud.descriptors.rows(); m++)
        cout<<m<<"  "<<mapCloud.descriptorLabels.at(m).text<<endl;

    for(int i=0; i<mapCloud.descriptors.cols(); i++)
    {
        if(mapCloud.descriptors(2, i) > max_session)
            max_session = mapCloud.descriptors(1, i);

        if(mapCloud.descriptors(2, i) < min_session)
            min_session = mapCloud.descriptors(1, i);

    }

    cout<<"max_session:  "<<max_session<<endl;
    cout<<"min_session:  "<<min_session<<endl;
    */

    /*
    int rowLine = mapCloud.getDescriptorStartingRow("session");
    staticCloud = mapCloud.createSimilarEmpty();
    int count = 0;
    int count1 =0;
    int count2 =0;

    for(int i=0; i<mapCloud.features.cols(); i++)
    {
        if(mapCloud.descriptors(rowLine, i) >= staticInt)
        {
            staticCloud.setColFrom(count, mapCloud, i);
            count++;
        }

        if(mapCloud.descriptors(rowLine, i) == 1)
            count1++;

        if(mapCloud.descriptors(rowLine, i) == 2)
            count2++;

    }

    staticCloud.conservativeResize(count);
    */


//    cout<<mapCloud.descriptors.rows()<<endl;
//    for(int i=0; i<mapCloud.descriptors.rows(); i++)
//    {
//        cout<<i<<"  "<<mapCloud.descriptorLabels.at(i).text<<endl;
//    }


//    for(int f=0; f<mapCloud.features.rows(); f++)
//        cout<<f<<":  "<<mapCloud.featureLabels.at(f).text<<endl;

//    for(int f=0; f<mapCloud.descriptors.rows(); f++)
//        cout<<f<<":  "<<mapCloud.descriptorLabels.at(f).text<<endl;


    //test
//    partCloud = mapCloud.createSimilarEmpty();
//    int count3 = 0;
//    for(int i=0; i<10000; i++)
//    {
//        partCloud.setColFrom(count3, mapCloud, i);
//        count3 ++;
//    }
//    partCloud.conservativeResize(count3);

    int rowLine = mapCloud.getDescriptorStartingRow("salient");
    staticCloud = mapCloud.createSimilarEmpty();
    int count=0;
    for(int i=0; i<mapCloud.features.cols(); i++)
    {
        if(mapCloud.descriptors(rowLine, i) == 1)
        {
            staticCloud.setColFrom(count, mapCloud, i);
            count++;
        }

    }

    staticCloud.conservativeResize(count);
    staticCloud.save("/home/yh/wholemap_salient.vtk");

    cout<<"map num:  "<<mapCloud.features.cols()<<endl;
    cout<<"static num:  "<<staticCloud.features.cols()<<endl;

    /** find MAX **/
    int rowLineRange = mapCloud.getDescriptorStartingRow("Range");
    int rowLineHeight = mapCloud.getDescriptorStartingRow("Height");

    double rangeMax = 0;
    double heightMax = 0;
    for(int m=0; m<mapCloud.features.cols(); m++)
    {
        mapCloud.descriptors(rowLineRange, m)>rangeMax?rangeMax=mapCloud.descriptors(rowLineRange, m):rangeMax=rangeMax;
        mapCloud.descriptors(rowLineHeight, m)>heightMax?heightMax=mapCloud.descriptors(rowLineHeight, m):heightMax=heightMax;
    }
    cout<<"range Max:   "<<rangeMax<<endl;
    cout<<"height Max:  "<<heightMax<<endl;

}

int main(int argc, char **argv)
{

    ros::init(argc, argv, "mapCheck");
    ros::NodeHandle n;

    mapCheck mapcheck(n);

    // ugly code

    return 0;
}