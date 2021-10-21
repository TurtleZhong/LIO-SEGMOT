// Copyright 2021 Yu-Kai Lin. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include <tuple>
#include <vector>

#include <gtsam/geometry/Point3.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/linear/NoiseModel.h>
#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam/slam/BetweenFactor.h>
#include <jsk_recognition_msgs/BoundingBox.h>
#include <Eigen/Dense>

class MaxMixturePoint3 : public gtsam::Point3 {};

class Detection {
 public:
  using BoundingBox = jsk_recognition_msgs::BoundingBox;

 protected:
  // TODO: Distinguish translation part and rotation part.
  gtsam::Point3 mu;
  gtsam::Vector3 sigmaVec;
  gtsam::Matrix33 sigmaMat;
  gtsam::Matrix33 info;
  gtsam::Matrix33 sqrtInfo;

  gtsam::noiseModel::Diagonal::shared_ptr diagonal;

  double w;

  BoundingBox box;

 public:
  Detection(BoundingBox box, gtsam::Vector3 sigma, double w = 1.0);
  Detection(BoundingBox box, double sigma = 1e-2, double w = 1.0);

  ///@name Gaussian Model
  ///@{

  const gtsam::Point3 getMu() const { return this->mu; }
  const gtsam::Vector3 getVarianceVec() const { return this->sigmaVec; }
  const gtsam::Matrix33 getVarianceMat() const { return this->sigmaMat; }
  const gtsam::Matrix33 getInformationMatrix() const { return this->info; }
  const gtsam::Matrix33 getSqrtInformationMatrix() const { return this->sqrtInfo; }

  const gtsam::noiseModel::Diagonal::shared_ptr getDiagonal() const { return this->diagonal; }

  const double getW() const { return this->w; }

  ///@}
  ///@name Bounding Box
  ///@{

  const BoundingBox getBoundingBox() const { return this->box; }

  ///@}
  ///@name Log-likelihood
  ///@{

  const double error(const gtsam::Vector3 x, const double gamma) const;

  ///@}
  ///@name State
  ///@{

  const gtsam::Pose3 getPose3() const;

  ///@}
};

class DetectionFactor : public gtsam::NonlinearFactor {
 private:
  using Key            = gtsam::Key;
  using Values         = gtsam::Values;
  using GaussianFactor = gtsam::GaussianFactor;

  enum class Mode {
    TIGHTLY_COUPLED,
    LOOSELY_COUPLED
  };

 protected:
  using This = DetectionFactor;
  using Base = gtsam::NonlinearFactor;

  Key detectionKey;
  Key robotPoseKey;

  std::vector<Detection> detections;
  std::vector<gtsam::noiseModel::Diagonal::shared_ptr> diagonals;
  std::vector<gtsam::Vector3> zs;

  double gamma;

  Mode mode;

 public:
  DetectionFactor(std::vector<Detection> detections,
                  Key detectionKey,
                  Key robotPoseKey,
                  Mode mode = Mode::TIGHTLY_COUPLED);

  DetectionFactor(const This *f);

  virtual ~DetectionFactor() {}

  ///@name Testable
  ///@{

  virtual void print(const std::string &s                    = "",
                     const gtsam::KeyFormatter &keyFormatter = gtsam::DefaultKeyFormatter) const override;

  virtual bool equals(const Base &f, double tol = 1e-9) const override;

  ///@}
  ///@name Standard Interface
  ///@{

  virtual double error(const Values &c) const override;

  virtual size_t dim() const override { return 3; };

  virtual GaussianFactor::shared_ptr linearize(const Values &c) const override;

  virtual Base::shared_ptr clone() const override;

  ///@}
  ///@name Max-Mixture
  ///@{

  virtual std::tuple<size_t, double>
  getDetectionIndexAndError(const gtsam::Pose3 &d) const;

  virtual std::tuple<size_t, double>
  getDetectionIndexAndError(const gtsam::Values &c) const;

  ///@}
  ///@name Utilities
  ///@{

  virtual const gtsam::Pose3 getDetectionValue(const gtsam::Values &c) const;

  virtual const gtsam::Pose3 getRobotPoseValue(const gtsam::Values &c) const;

  ///@}
};

class ConstantVelocityFactor : public gtsam::BetweenFactor<gtsam::Pose3> {
 private:
  using This = ConstantVelocityFactor;
  using Base = gtsam::BetweenFactor<gtsam::Pose3>;

 public:
  ConstantVelocityFactor(gtsam::Key key1,
                         gtsam::Key key2,
                         const gtsam::SharedNoiseModel &model = nullptr)
      : Base(key1, key2, gtsam::Pose3::identity(), model) {
  }

  virtual ~ConstantVelocityFactor() {}

  ///@name Testable
  ///@{

  /** print */
  virtual void
  print(const std::string &s,
        const gtsam::KeyFormatter &keyFormatter = gtsam::DefaultKeyFormatter) const {
    std::cout << s << "ConstantVelocityFactor("
              << keyFormatter(this->key1()) << ","
              << keyFormatter(this->key2()) << ")\n";
    this->noiseModel_->print("  noise model: ");
  }

  /** equals */
  virtual bool equals(const NonlinearFactor &expected, double tol = 1e-9) const {
    const This *e = dynamic_cast<const This *>(&expected);
    return e != NULL && Base::equals(*e, tol);
  }
};

class StablePoseFactor : public gtsam::NoiseModelFactor3<gtsam::Pose3,
                                                         gtsam::Pose3,
                                                         gtsam::Pose3> {
 private:
  using This = StablePoseFactor;
  using Base = gtsam::NoiseModelFactor3<gtsam::Pose3,
                                        gtsam::Pose3,
                                        gtsam::Pose3>;

 public:
  /** Constructor */
  StablePoseFactor(gtsam::Key previousPoseKey,
                   gtsam::Key velocityKey,
                   gtsam::Key nextPoseKey,
                   const gtsam::SharedNoiseModel &model = nullptr)
      : Base(model, previousPoseKey, velocityKey, nextPoseKey) {
  }

  /** print */
  virtual void
  print(const std::string &s,
        const gtsam::KeyFormatter &keyFormatter = gtsam::DefaultKeyFormatter) const {
    std::cout << s << "StablePoseFactor("
              << keyFormatter(this->key1()) << ","
              << keyFormatter(this->key2()) << ","
              << keyFormatter(this->key3()) << ")\n";
    this->noiseModel_->print("  noise model: ");
  }

  /** equals */
  virtual bool equals(const NonlinearFactor &expected, double tol = 1e-9) const {
    const This *e = dynamic_cast<const This *>(&expected);
    return e != NULL && Base::equals(*e, tol);
  }

  inline gtsam::Key previousPoseKey() const { return key1(); }
  inline gtsam::Key velocityKey() const { return key2(); }
  inline gtsam::Key nextPoseKey() const { return key3(); }

  virtual gtsam::Vector
  evaluateError(const gtsam::Pose3 &previousPose,
                const gtsam::Pose3 &velocity,
                const gtsam::Pose3 &nextPose,
                boost::optional<gtsam::Matrix &> H1 = boost::none,
                boost::optional<gtsam::Matrix &> H2 = boost::none,
                boost::optional<gtsam::Matrix &> H3 = boost::none);
};