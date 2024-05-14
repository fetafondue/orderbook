#ifndef LEVELINFO_H
#define LEVELINFO_H

struct LevelInfo
{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

#endif