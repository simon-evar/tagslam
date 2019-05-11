/* -*-c++-*--------------------------------------------------------------------
 * 2019 Bernd Pfrommer bernd.pfrommer@gmail.com
 */

#include "tagslam/factor/distance.h"
#include "tagslam/graph.h"
#include "tagslam/body.h"
#include <geometry_msgs/Point.h>
#include <XmlRpcException.h>
#include <string>
#include <sstream>
#include <boost/range/irange.hpp>

namespace tagslam {
  namespace factor {
    using boost::irange;
    
    Distance::Distance(double dist,  double noise,
                       const int corn1, const Tag2ConstPtr &tag1,
                       const int corn2, const Tag2ConstPtr &tag2,
                       const std::string  &name) :
      Factor(name, ros::Time(0)), distance_(dist), noise_(noise) {
      tag_[0] = tag1; tag_[1] = tag2;
      corner_[0] = corn1; corner_[1] = corn2;
    }

    VertexDesc Distance::addToGraph(const VertexPtr &vp, Graph *g) const {
      const ros::Time t0 = ros::Time(0);
      const VertexDesc vt1p = g->findTagPose(getTag(0)->getId());
      checkIfValid(vt1p, "tag pose 1 not found");
      const VertexDesc vt2p = g->findTagPose(getTag(1)->getId());
      checkIfValid(vt2p, "tag pose 2 not found");
      const VertexDesc vb1p = g->findBodyPose(t0,
                                              getTag(0)->getBody()->getName());
      checkIfValid(vb1p, "body pose 1 not found");
      const VertexDesc vb2p = g->findBodyPose(t0,
                                              getTag(1)->getBody()->getName());
      checkIfValid(vb2p, "body pose 2 not found");
      const VertexDesc fv = g->insertFactor(vp);
      g->addEdge(fv, vb1p, 0);
      g->addEdge(fv, vt1p, 1);
      g->addEdge(fv, vb2p, 2);
      g->addEdge(fv, vt2p, 3);
      return (fv);
    }
    
    void Distance::addToOptimizer(Graph *g) const {
      const VertexDesc v = g->find(this);
      checkIfValid(v, "factor not found");
      const std::vector<ValueKey> optKeys = g->getOptKeysForFactor(v, 4);
      const FactorKey fk = g->getOptimizer()->addDistanceMeasurement(
        getDistance(), getNoise(), getCorner(0),
        optKeys[0] /* T_w_b1 */,  optKeys[1] /* T_b1_o */,
        getCorner(1), optKeys[2] /* T_w_b2 */, optKeys[3] /* T_b2_o */);
      g->markAsOptimized(v, fk);
    }
    
    double Distance::distance(
      const Transform &T_w_b1, const Transform &T_b1_o,
      const Transform &T_w_b2, const Transform &T_b2_o) const {
      const auto X1 = T_w_b1 * T_b1_o * getCorner(0);
      const auto X2 = T_w_b2 * T_b2_o * getCorner(1);
      return ((X2 - X1).norm());
    }

    const Eigen::Vector3d Distance::getCorner(int idx) const {
      return (tag_[idx]->getObjectCorner(corner_[idx]));
    }

    DistanceFactorPtr
    Distance::parse(const std::string &name, XmlRpc::XmlRpcValue meas,
                    TagFactory *tagFactory) {
      int tag1(-1), tag2(-1), c1(-1), c2(-1);
      double d(-1), noise(-1);
      try {
        for (XmlRpc::XmlRpcValue::iterator it = meas.begin();
             it != meas.end(); ++it) {
          if (it->first == "tag1") { tag1 = static_cast<int>(it->second); }
          if (it->first == "tag2") { tag2 = static_cast<int>(it->second); }
          if (it->first == "corner1") { c1 = static_cast<int>(it->second); }
          if (it->first == "corner2") { c2 = static_cast<int>(it->second); }
          if (it->first == "distance") { d = static_cast<double>(it->second); }
          if (it->first == "noise") { noise = static_cast<double>(it->second);}
        }
      } catch (const XmlRpc::XmlRpcException &e) {
        throw std::runtime_error("error parsing measurement:" + name);
      }
      DistanceFactorPtr fp;
      if (tag1 >= 0 && tag2 >= 0 && c1 >= 0 && c2 >= 0 && d > 0 && noise > 0) {
        Tag2ConstPtr tag1Ptr = tagFactory->findTag(tag1);
        Tag2ConstPtr tag2Ptr = tagFactory->findTag(tag2);
        if (!tag1Ptr || !tag2Ptr) {
          ROS_ERROR_STREAM("measured tags are not valid: " << name);
          throw (std::runtime_error("measured tags are not valid!"));
        }
        fp.reset(new factor::Distance(d, noise, c1, tag1Ptr, c2, tag2Ptr,
                                      name));
      } else {                                  
        ROS_ERROR_STREAM("distance measurement incomplete: " << name);
        throw (std::runtime_error("distance measurement incomplete!"));
      }
      return (fp);
    }

    DistanceFactorPtrVec
    Distance::parse(XmlRpc::XmlRpcValue meas, TagFactory *tagFactory) {
      DistanceFactorPtrVec dv;
      if (meas.getType() == XmlRpc::XmlRpcValue::TypeInvalid) {
        throw std::runtime_error("invalid node type for measurement!");
      }
      for (const auto i: irange(0, meas.size())) {
        if (meas[i].getType() != XmlRpc::XmlRpcValue::TypeStruct) continue;
        for (XmlRpc::XmlRpcValue::iterator it = meas[i].begin();
             it != meas[i].end(); ++it) {
          if (it->second.getType() != XmlRpc::XmlRpcValue::TypeStruct) {
            continue;
          }
          DistanceFactorPtr d = parse(it->first, it->second, tagFactory);
          if (d) {
            dv.push_back(d);
          }
        }
      }
      return (dv);
    }
    
    std::string Distance::getLabel() const {
      std::stringstream ss;
      ss << name_;
      return (ss.str());
    }
  } // namespace factor
}  // namespace tagslam