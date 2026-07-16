#pragma once

#include <memory>

#include "libcommonserver/db/db.h"

namespace KDUpdater {

class ParmsDbLite : public KDC::Db {
    public:
        explicit ParmsDbLite(const std::filesystem::path &dbPath) :
            Db(dbPath) {}

        static std::shared_ptr<ParmsDbLite> instance(const std::filesystem::path &dbPath);

        bool selectAppUid(std::string &appUid, bool &found);
        std::string dbType() const override { return "ParmsDbLite"; }

        bool create(bool &retry) override;
        bool prepare() override;
        bool upgrade(const std::string &fromVersion, const std::string &toVersion) override;
};

} // namespace KDUpdater
