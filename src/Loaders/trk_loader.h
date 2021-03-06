//
// Created by Amrik.Sadhra on 20/06/2018.
//

#pragma once

#include <memory>
#include "../nfs_data.h"
#include "nfs3_loader.h"
#include "nfs2_loader.h"
#include "nfs4_loader.h"
#include <boost/variant.hpp>

class ONFSTrack {
public:
    explicit ONFSTrack(NFSVer nfs_version, const std::string &track_name);

    ~ONFSTrack() {
        switch (tag) {
            case NFS_2:
            case NFS_2_SE:
            case NFS_3_PS1:
            case NFS_4:
            case NFS_3:
                break;/*
            case NFS_3:
                NFS3::FreeTrack(boost::get<shared_ptr<NFS3_4_DATA::TRACK>>(trackData));
                break;*/
            case UNKNOWN:
                ASSERT(false, "Unknown track type!");
            default:
                break;
        }
    }

    NFSVer tag;
    std::string name;
    typedef boost::variant<shared_ptr<NFS3_4_DATA::TRACK>, shared_ptr<NFS2_DATA::PS1::TRACK>, shared_ptr<NFS2_DATA::PC::TRACK>> track;
    track trackData;

    std::vector<SHARED::CANPT> camera_animations;
    std::vector<TrackBlock> track_blocks;
    std::vector<Entity> global_objects;
    uint32_t nBlocks;
    GLuint textureArrayID;
};

class TrackLoader {
public:
    static shared_ptr<ONFSTrack> LoadTrack(NFSVer nfs_version, const std::string &track_name);
};