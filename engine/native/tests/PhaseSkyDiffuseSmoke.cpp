#include "../shaders/PhaseSkyDiffuse.hlsli"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr double Pi = 3.14159265358979323846;
void Need(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Sample { double x, weight; };
// 獨立 oracle：Gauss-Legendre 積分節點，不使用候選的級數係數。
std::vector<Sample> Quadrature(int count) {
    std::vector<Sample> result;
    for (int i = 1; i <= count; ++i) {
        double x = std::cos(Pi * (double(i) - .25) / (double(count) + .5));
        double derivative = 0;
        for (int iteration = 0; iteration < 32; ++iteration) {
            double a = 1, b = x;
            for (int n = 2; n <= count; ++n) {
                const double next = ((2 * n - 1) * x * b - (n - 1) * a) / n;
                a = b; b = next;
            }
            derivative = count * (x * b - a) / (x * x - 1);
            const double dx = b / derivative;
            x -= dx;
            if (std::abs(dx) < 1e-15) break;
        }
        result.push_back({(x + 1) * .5, 1 / ((1 - x * x) * derivative * derivative)});
    }
    return result;
}
double AzimuthCosine(double normalY, double skyY) {
    const double a = normalY * skyY;
    const double b = std::sqrt((std::max)(0.0, (1 - normalY * normalY) * (1 - skyY * skyY)));
    if (a >= b) return 2 * a;
    if (a <= -b) return 0;
    return 2 / Pi * (a * std::acos(-a / b) + std::sqrt(b * b - a * a));
}
double ReferencePole(double normalY, double exponent, const std::vector<Sample>& samples) {
    double value = 0;
    // t=y^(k+1) 消除地平線 power 的導數奇異點；方位角 cosine 積分解析求值。
    for (const auto& sample : samples)
        value += sample.weight * AzimuthCosine(normalY, std::pow(sample.x, 1 / (exponent + 1))) / (exponent + 1);
    return value;
}
}
int main() {
    try {
        const auto coarse = Quadrature(128), fine = Quadrature(256);
        double maxError = 0, oracleDelta = 0, defaultError = 0;
        double rawMinimum = 0, meanEnergyError = 0, rawError = 0;
        unsigned checks = 0;
        std::vector<float> normals;
        for(int step=0;step<=128;++step) normals.push_back(float(step)/64.f-1.f);
        for(float y:{-.9999f,-.999f,-.99f,.99f,.999f,.9999f}) normals.push_back(y);
        Need(std::abs(ReferencePole(0,1,fine)-2/(3*Pi))<2e-6,"horizontal linear-power exact oracle");
        for (float k : {.05f,.1f,.2f,.3f,.325f,.35f,.375f,.4f,.42f,.45f,.75f,1.f,2.f,3.f,4.f,6.f,8.f}) {
            for (float y : normals) {
                const auto w = Aevrynd::EvaluatePhaseSkyDiffuseWeights(y, k);
                const auto raw = Aevrynd::EvaluatePhaseSkyDiffuseRawWeights(y, k);
                const auto mirror = Aevrynd::EvaluatePhaseSkyDiffuseWeights(-y, k);
                const double up = ReferencePole(y,k,fine), down = ReferencePole(-y,k,fine);
                const double error = (std::max)({std::abs(w.zenith-up),std::abs(w.ground-down),std::abs(w.horizon-(1-up-down))});
                maxError = (std::max)(maxError,error);
                rawMinimum=(std::min)({rawMinimum,double(raw.zenith),double(raw.horizon),double(raw.ground)});
                rawError=(std::max)({rawError,std::abs(raw.zenith-up),std::abs(raw.ground-down),std::abs(raw.horizon-(1-up-down))});
                oracleDelta = (std::max)(oracleDelta,std::abs(up-ReferencePole(y,k,coarse)));
                Need(std::isfinite(w.zenith) && std::isfinite(w.horizon) && std::isfinite(w.ground),"finite weights");
                Need(w.zenith>=0 && w.horizon>=0 && w.ground>=0,"nonnegative radiance");
                Need(std::abs(w.zenith+w.horizon+w.ground-1.f)<3e-7f,"constant sky energy");
                Need(std::abs(w.zenith-mirror.ground)<2e-7f && std::abs(w.horizon-mirror.horizon)<2e-7f,"hemisphere symmetry");
                // 所有正規化 power profile 的絕對權重誤差先固定在 0.5% 內。
                Need(error < .005,"hemisphere integration error");
                if(k==2.f || k==4.f || k==6.f || k==8.f) Need(error<3e-6,"finite even polynomial exactness");
                if (k == .42f) {
                    const std::array<double,3> z{.16,.28,.43},h{.62,.67,.69},g{.16,.15,.12};
                    for (unsigned c=0;c<3;++c) {
                        const double actual=w.zenith*z[c]+w.horizon*h[c]+w.ground*g[c];
                        const double expected=up*z[c]+(1-up-down)*h[c]+down*g[c];
                        defaultError=(std::max)(defaultError,std::abs(actual-expected));
                    }
                }
                ++checks;
            }
            Need(std::abs(ReferencePole(1,k,fine)-2/(double(k)+2))<2e-5,"upward analytic oracle");
            Need(ReferencePole(-1,k,fine)==0,"downward analytic oracle");
            std::array<double,3> mean{};
            for(const auto& sample:fine) {
                const auto w=Aevrynd::EvaluatePhaseSkyDiffuseWeights(float(sample.x*2-1),k);
                mean[0]+=sample.weight*w.zenith;
                mean[1]+=sample.weight*w.horizon;
                mean[2]+=sample.weight*w.ground;
            }
            meanEnergyError=(std::max)({meanEnergyError,std::abs(mean[0]-.5/(double(k)+1)),
                std::abs(mean[1]-double(k)/(double(k)+1)),std::abs(mean[2]-.5/(double(k)+1))});
        }
        std::cout.precision(12);
        std::cout << "PHASE_SKY_DIFFUSE_MEASURE checks=" << checks << " max_weight_error=" << maxError
          << " oracle_delta=" << oracleDelta << " default_radiance_error=" << defaultError
          << " raw_minimum=" << rawMinimum << " raw_error=" << rawError << " mean_energy_error=" << meanEnergyError << '\n';
        Need(oracleDelta<3e-5,"independent quadrature convergence");
        Need(defaultError<.001,"default sky absolute radiance error");
        Need(meanEnergyError<.001,"bounded clamp energy bias");
        const auto up=Aevrynd::EvaluatePhaseSkyDiffuseWeights(1,.42f);
        const double actual=up.zenith*.16+up.horizon*.62+up.ground*.16;
        Need(actual>.23 && actual<.25,"upward red includes horizon irradiance");
        std::cout.precision(12);
        std::cout << "PHASE_SKY_DIFFUSE_PASS checks=" << checks << " max_weight_error=" << maxError
          << " oracle_delta=" << oracleDelta << " default_radiance_error=" << defaultError
          << " upward_red=" << actual << " legacy_upward_red_without_sunwrap=0.16\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
