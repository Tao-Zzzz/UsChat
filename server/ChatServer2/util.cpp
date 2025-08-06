
#include "util.h"

std::string getCurrentTimestamp() {
    namespace pt = boost::posix_time;
    // ȡ��ǰ����ʱ�䣨��ȷ���룩
    pt::ptime now = pt::second_clock::local_time();
    // �� time_facet ָ����ʽ
    std::ostringstream oss;
    static std::locale loc(std::locale::classic(),
        new pt::time_facet("%Y-%m-%d %H:%M:%S"));
    oss.imbue(loc);
    oss << now;
    return oss.str();
}