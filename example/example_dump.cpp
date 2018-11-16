#include <memory>
#include <fstream>

// unix
#include "sys/stat.h"

// 3rdparty
#include "folly/Format.h"
#include "opencv2/imgproc.hpp"

// feh
#include "dataloader.h"
#include "utils.h"

const int downsample_rate = 2;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << feh::TermColor::red
                  << "Usage: example_dump DIRECTORY_OF_THE_DATASET OUTPUT_DIRECTORY"
                  << feh::TermColor::endl;
        exit(-1);
    }

    std::shared_ptr<feh::VlslamDatasetLoader> loader;
    try {
        loader = std::make_shared<feh::VlslamDatasetLoader>(argv[1]);
    } catch (const std::exception &) {
        std::cout << feh::TermColor::red
                  << "failed to initialize dataset @ " << argv[1] << feh::TermColor::endl;
        exit(-1);
    }

    // create directory
    const mode_t DIR_MODE = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    if (mkdir(argv[2], DIR_MODE) < 0) {
        std::cout << feh::TermColor::red
                  << "Failed to create output directory" << feh::TermColor::endl;
        exit(-1);
    }

    auto ss = folly::sformat("{}/image", argv[2]);
    if (mkdir(ss.c_str(), DIR_MODE) < 0) {
        std::cout << feh::TermColor::red
                  << "Failed to create output/image directory" << feh::TermColor::endl;
        exit(-1);
    }
    ss = folly::sformat("{}/pose", argv[2]);
    if (mkdir(ss.c_str(), DIR_MODE) < 0) {
        std::cout << feh::TermColor::red
                  << "Failed to create output/pose directory" << feh::TermColor::endl;
        exit(-1);
    }
    ss = folly::sformat("{}/depth", argv[2]);
    if (mkdir(ss.c_str(), DIR_MODE) < 0) {
        std::cout << feh::TermColor::red
                  << "Failed to create output/depth directory" << feh::TermColor::endl;
        exit(-1);
    }

    std::array<int, 2> size;
    std::vector<float> params;
    loader->GrabCameraInfo(size, params);
    for (auto each : params) {
        std::cout << each << " ";
    }

    Eigen::Matrix3f K;
    // parameters 
    K << params[0], 0, params[2],
      0, params[1], params[3],
      0, 0, 1;
    std::ofstream Kout(folly::sformat("{}/K.txt", argv[2]), std::ios::out);
    Kout << K;
    Kout.close();

    for (int i = 0; i < loader->size(); ++i) {
        Sophus::SE3f gwc;
        Sophus::SO3f Rg;    // rotation to align with gravity
        cv::Mat img, edgemap;
        vlslam_pb::BoundingBoxList bboxlist;

        loader->Grab(i, img, edgemap, bboxlist, gwc, Rg);   // grab datum
        auto depth_samples = loader->GrabSparseDepth(i);

        // std::cout << "g(world <- camera)=\n" << gwc.matrix3x4() << std::endl;
        // std::cout << "Rg=\n" << Rg.matrix() << std::endl;

        // write out pose
        std::ofstream fid_pose;
        fid_pose.open(folly::sformat("{}/pose/{:06d}.txt", argv[2], i));
        fid_pose << gwc.matrix();
        fid_pose.close();

        // write out sparse depth
        std::ofstream fid_depth;
        try {
            fid_depth.open(folly::sformat("{}/depth/{:06d}.txt", argv[2], i));
            for (const auto &s : depth_samples) {
                if (s.second[1] > 0) {
                    fid_depth << s.second[0] << " " 
                              << s.second[1] << " " 
                              << s.second[2] << std::endl;
                }
            }
            fid_depth.close();
        } catch (const std::exception &) {
            exit(-1);
        }

        // write out image
        cv::imwrite(folly::sformat("{}/image/{:06d}.jpg", argv[2], i), img);

        cv::imshow("image", img);
        // cv::imshow("edge map", edgemap);
        char ckey = cv::waitKey(30);
        if (ckey == 'q') break;
    }
}
