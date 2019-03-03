//
// Created by amrik on 01/03/19.
//

#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "../Scene/Camera.h"
#include "../Physics/PhysicsEngine.h"
#include "../Loaders/car_loader.h"
#include "../Loaders/trk_loader.h"
#include "../RaceNet/CarAgent.h"
#include "../Util/Logger.h"
#include "../Config.h"
#include "../Renderer/Renderer.h"

class RaceSession {
private:
    GLFWwindow *window;
    std::shared_ptr<Logger> logger;
    std::vector<NeedForSpeed> installedNFSGames;
    AssetData loadedAssets;
    shared_ptr<ONFSTrack> track;
    shared_ptr<Car> car;
    PhysicsEngine physicsEngine;
    Camera mainCamera;
    ParamData userParams;
    Renderer mainRenderer = Renderer(window, logger, installedNFSGames, track, car);

    uint64_t ticks = 0; // Engine ticks elapsed
    float totalTime = 0;
public:
    RaceSession(GLFWwindow *glWindow, std::shared_ptr<Logger> &onfsLogger, const std::vector<NeedForSpeed> &installedNFS, const shared_ptr<ONFSTrack> &currentTrack, shared_ptr<Car> &currentCar);
    AssetData simulate();
};
