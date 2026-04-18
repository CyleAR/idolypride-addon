#ifndef IDOLYPRIDE_LOCALIFY_MASTERLOCAL_H
#define IDOLYPRIDE_LOCALIFY_MASTERLOCAL_H

#include <string>

namespace HoshimiLocal::MasterLocal {
    void LoadData();

    void LocalizeMasterItem(void* item, const std::string& tableName);
}

#endif //IDOLYPRIDE_LOCALIFY_MASTERLOCAL_H
