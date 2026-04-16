#pragma once
#include <string>

namespace HoshimiLocal::MasterLocal {
    void LoadData();
    void LocalizeMasterItem(void* item, const std::string& tableName);
}
