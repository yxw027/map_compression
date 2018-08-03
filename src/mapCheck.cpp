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
    ros::Publisher pathCloudPub;

    int staticInt;
    DP mapCloud;
    DP staticCloud;
    DP pathCloud;

    nav_msgs::Path mapPath;
    string keepIndexName;
    vector<int> indexVector;
    vector<vector<double>> initPoses;

    int isScore;

    void process();

};

mapCheck::~mapCheck()
{}

mapCheck::mapCheck(ros::NodeHandle& n):
    n(n),
    loadMapName(getParam<string>("loadMapName", ".")),
    staticInt(getParam<int>("staticInt", 0)),
    loadTrajName(getParam<string>("loadTrajName", ".")),
    isScore(getParam<int>("isScore", 0)),
    keepIndexName(getParam<string>("keepIndexName", "."))
{
    mapCloudPub = n.advertise<sensor_msgs::PointCloud2>("mapCloud", 2, true);
    staticCloudPub = n.advertise<sensor_msgs::PointCloud2>("staticCloud", 2, true);
    mapPathPub = n.advertise<nav_msgs::Path>("mapPath", 20000, true);
    pathCloudPub = n.advertise<sensor_msgs::PointCloud2>("pathCloud", 2, true);

    // load
    mapCloud = DP::load(loadMapName);

    // read initial transformation
    int x, y;
    double temp;
    vector<double> test;
    ifstream in(loadTrajName);  // loadTrajName == icpFileName
    if (!in) {
        cout << "Cannot open file.\n";
    }
    for (y = 0; y < 9999999; y++) {
        test.clear();
    for (x = 0; x < 16; x++) {
      in >> temp;
      test.push_back(temp);
    }
      initPoses.push_back(test);
    }
    in.close();

    // read all the effective index from list in the txt
    int l;
    ifstream in_(keepIndexName);
    if (!in_) {
        cout << "Cannot open file.\n";
    }
    while(!in_.eof())
    {
        in_>>l;
        indexVector.push_back(l);
    }

    // read initial transformation
    mapPath.header.frame_id = "global";
    mapPath.header.stamp = ros::Time::now();
    for (y = 0; y < indexVector.size(); y++)
    {
        int index =indexVector.at(y);

        geometry_msgs::PoseStamped pose;
        pose.header.frame_id = mapPath.header.frame_id;
        pose.header.stamp = mapPath.header.stamp;
        pose.pose.position.x = initPoses[index][3];
        pose.pose.position.y = initPoses[index][7];
        pose.pose.position.z = initPoses[index][11];
        pose.pose.orientation = tf::createQuaternionMsgFromYaw(0);  // always 0;

        mapPath.poses.push_back(pose);
    }

    // transfer the path to thecloud
    pathCloud = mapCloud.createSimilarEmpty();
    for(int p = 0; p< indexVector.size(); p++)
    {
        int pindex =indexVector.at(p);
        pathCloud.features(0, p) = initPoses[pindex][3];
        pathCloud.features(1, p) = initPoses[pindex][7];
        pathCloud.features(2, p) = initPoses[pindex][11];
    }
    pathCloud.conservativeResize(indexVector.size());

    // process
    this->process();

    staticCloudPub.publish(PointMatcher_ros::pointMatcherCloudToRosMsg<float>(staticCloud, "global", ros::Time(0)));
    mapCloudPub.publish(PointMatcher_ros::pointMatcherCloudToRosMsg<float>(mapCloud, "global", ros::Time(0)));
    mapPathPub.publish(mapPath);
    pathCloudPub.publish(PointMatcher_ros::pointMatcherCloudToRosMsg<float>(pathCloud, "global", ros::Time(0)));
}

void mapCheck::process()
{
    /**  CHECK TEST  **/

    /**SALIENT**/
    if(!isScore)
    {
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
    }
    else
    {
        /**SESSIONS**/

        int rowLine = mapCloud.getDescriptorStartingRow("session");
        staticCloud = mapCloud.createSimilarEmpty();
        int count=0;
        for(int i=0; i<mapCloud.features.cols(); i++)
        {
            if(mapCloud.descriptors(rowLine, i) >= staticInt)
            {
                staticCloud.setColFrom(count, mapCloud, i);
                count++;
            }

        }

        staticCloud.conservativeResize(count);
    }

    cout<<"map num:  "<<mapCloud.features.cols()<<endl;
//    cout<<"salient num:  "<<staticCloud.features.cols()<<endl;
    cout<<"static num:  "<<staticCloud.features.cols()<<endl;

}

int main(int argc, char **argv)
{

    ros::init(argc, argv, "mapCheck");
    ros::NodeHandle n;

    mapCheck mapcheck(n);

    // ugly code

    return 0;
}
