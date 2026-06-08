#include <iostream>
#include <iomanip>
#include <cmath>
#include <random>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>

double catenary_z(double x, double x1, double x2, double ah, double sag) {
    double span = (x2 - x1) / 2.0, cx = (x1 + x2) / 2.0;
    return ah - sag * (1 - std::pow((x - cx) / span, 2));
}

auto gen_pts(double x1, double x2, double cy, double ah, double sag,
             int n, double noise, std::mt19937& rng) {
    std::normal_distribution<> g(0.0, noise);
    auto cloud = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud<pcl::PointXYZ>);
    cloud->resize(n);
    for (int i = 0; i < n; i++) {
        double x = x1 + (double)i/(n-1)*(x2-x1);
        (*cloud)[i] = pcl::PointXYZ(x+g(rng), cy+g(rng), catenary_z(x, x1, x2, ah, sag)+g(rng));
    }
    return cloud;
}

int main() {
    double x1=-30, x2=30, cy=0.6, ah=25, sag=4;
    std::mt19937 rng(42);

    std::cout << "=== 导线点云 RANSAC 离线验证 ===\n\n";
    std::cout << "导线: x=[" << x1 << "," << x2 << "], y=" << cy
              << ", 附着高=" << ah << "m, 垂度=" << sag << "m\n\n";

    // Short segment (near-straight)
    auto test_seg = [&](double sx1, double sx2, const char* label) {
        double z1 = catenary_z(sx1, x1, x2, ah, sag);
        double z2 = catenary_z(sx2, x1, x2, ah, sag);
        double len = std::hypot(sx2-sx1, z2-z1);
        double gnx = (sx2-sx1)/len, gnz = (z2-z1)/len;

        std::cout << "--- " << label << " [" << sx1 << "," << sx2 << "] ---\n";
        std::cout << "    真实方向: (" << gnx << ", 0, " << gnz << ")\n\n";

        for (double sigma : {0.02, 0.05, 0.1, 0.2}) {
            auto cloud = gen_pts(sx1, sx2, cy, ah, sag, 200, sigma, rng);

            pcl::SACSegmentation<pcl::PointXYZ> seg;
            pcl::ModelCoefficients coeff;
            pcl::PointIndices inliers;
            seg.setOptimizeCoefficients(true);
            seg.setModelType(pcl::SACMODEL_LINE);
            seg.setMethodType(pcl::SAC_RANSAC);
            seg.setDistanceThreshold(sigma*3);
            seg.setMaxIterations(1000);
            seg.setInputCloud(cloud);
            seg.segment(inliers, coeff);

            double ir = 0, rmse = 0, ang = -1;
            if (!inliers.indices.empty()) {
                ir = (double)inliers.indices.size() / cloud->size();
                double px=coeff.values[0], py=coeff.values[1], pz=coeff.values[2];
                double nx=coeff.values[3], ny=coeff.values[4], nz=coeff.values[5];
                double sq = 0;
                for (int idx : inliers.indices) {
                    auto& p = (*cloud)[idx];
                    double ddx=p.x-px, ddy=p.y-py, ddz=p.z-pz;
                    double dot = ddx*nx + ddy*ny + ddz*nz;
                    sq += ddx*ddx + ddy*ddy + ddz*ddz - dot*dot;
                }
                rmse = std::sqrt(sq / inliers.indices.size());
                ang = std::acos(std::clamp(nx*gnx+nz*gnz, -1.0, 1.0)) * 180.0 / M_PI;
            }

            std::cout << "  σ=" << sigma << "m -> 内点率=" << std::setprecision(1)
                      << ir*100 << "%, RMSE=" << std::setprecision(3) << rmse
                      << "m, 方向角差=" << ang << "°\n";
        }
        std::cout << "\n";
    };

    test_seg(-10, 10, "局部短弧");
    test_seg(-5, 5, "更短弧段");

    // Full cable - catenary
    std::cout << "--- 整条导线 [" << x1 << "," << x2 << "] ---\n";
    auto full = gen_pts(x1, x2, cy, ah, sag, 500, 0.1, rng);
    pcl::SACSegmentation<pcl::PointXYZ> seg;
    pcl::ModelCoefficients coeff;
    pcl::PointIndices inliers;
    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_LINE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(0.3);
    seg.setMaxIterations(1000);
    seg.setInputCloud(full);
    seg.segment(inliers, coeff);
    std::cout << "  直线拟合: 内点率=" << (double)inliers.indices.size()/500*100
              << "% (预期低: 抛物线非直线)\n\n";

    std::cout << "========================================\n";
    std::cout << "结论: 直线模型对弧线导线效果有限,\n";
    std::cout << "后续需用 catenary/spline + Frenet 框架\n";
    std::cout << "========================================\n";
    return 0;
}
