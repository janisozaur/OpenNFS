//
// Created by amrik on 28/02/19.
//

#include "CarAgent.h"

CarAgent::CarAgent(uint16_t populationID, const std::shared_ptr<Car> &trainingCar, const std::shared_ptr<ONFSTrack> &trainingTrack) : track(trainingTrack), name("TrainingAgent" + std::to_string(populationID)) {
    this->populationID = populationID;
    training = true;
    fitness = 0;
    tickCount = 0;
    dead = false;

    this->car = std::make_shared<Car>(trainingCar->allModels, trainingCar->tag, trainingCar->name);
    this->car->colour = glm::vec3(Utils::RandomFloat(0.f, 1.f), Utils::RandomFloat(0.f, 1.f), Utils::RandomFloat(0.f, 1.f));
}

CarAgent::CarAgent(const std::string &racerName, const std::string &networkPath, const std::shared_ptr<Car> &car, const std::shared_ptr<ONFSTrack> &trainingTrack) : track(trainingTrack) {
    if (boost::filesystem::exists(networkPath)) {
        raceNet.import_fromfile(networkPath);
    } else {
        LOG(WARNING) << "AI Neural network couldn't be loaded from " << BEST_NETWORK_PATH << ", randomising weights";
    }
    name = racerName;
    this->car = std::make_shared<Car>(car->allModels, car->tag, car->name);
}

void CarAgent::resetToVroad(int trackBlockIndex, int posIndex, float offset, std::shared_ptr<ONFSTrack> &track, std::shared_ptr<Car> &car) {
    glm::vec3 vroadPoint;
    glm::quat carOrientation;

    ASSERT(offset <= 1.f, "Cannot reset to offset larger than +- 1.f on VROAD (Will spawn off track!)");

    if (track->tag == NFS_3 || track->tag == NFS_4) {
        // Can move this by trk[trackBlockIndex].nodePositions
        uint32_t nodeNumber = boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->trk[trackBlockIndex].nStartPos;
        int nPositions = boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->trk[trackBlockIndex].nPositions;
        if (posIndex <= nPositions){
            nodeNumber += posIndex;
        } else {
            // Advance the trackblock until we can get to posIndex
            int nExtra = posIndex - nPositions;
            while(true){
                nodeNumber = boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->trk[++trackBlockIndex].nStartPos;
                nPositions = boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->trk[trackBlockIndex].nPositions;
                if(nExtra < nPositions){
                    nodeNumber += nExtra;
                    break;
                } else {
                    nExtra -= nPositions;
                }
            }
        }
        glm::quat rotationMatrix = glm::normalize(glm::quat(glm::vec3(-SIMD_PI / 2, 0, 0)));
        COLVROAD resetVroad = boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->col.vroad[nodeNumber];
        vroadPoint = (rotationMatrix * TrackUtils::pointToVec(resetVroad.refPt)) / 65536.f;
        vroadPoint /= 10.f;
        vroadPoint.y += 0.2;

        // Get VROAD right vector
        glm::vec3 curVroadRightVec = rotationMatrix * glm::vec3(resetVroad.right.x / 128.f, resetVroad.right.y / 128.f, resetVroad.right.z / 128.f);
        vroadPoint += offset * curVroadRightVec;

        rotationMatrix = glm::normalize(glm::quat(glm::vec3(SIMD_PI / 2, 0, 0)));
        glm::vec3 forward = TrackUtils::pointToVec(resetVroad.forward) * rotationMatrix;
        glm::vec3 normal = TrackUtils::pointToVec(resetVroad.normal) * rotationMatrix;
        carOrientation = glm::conjugate(glm::toQuat(
                glm::lookAt(vroadPoint,
                            vroadPoint - forward,
                            normal
                )
        ));
    } else {
        vroadPoint = TrackUtils::pointToVec(track->trackBlocks[trackBlockIndex].center);
        vroadPoint.y += 0.2;
        carOrientation = glm::quat(2, 0, 0, 1);
    }

    // Go and find the Vroad Data to reset to
    car->resetCar(vroadPoint, carOrientation);
}

void CarAgent::resetToVroad(int vroadIndex, float offset, std::shared_ptr<ONFSTrack> &track, std::shared_ptr<Car> &car)  {
    glm::vec3 vroadPoint;
    glm::quat carOrientation;

    ASSERT(offset <= 1.f, "Cannot reset to offset larger than +- 1.f on VROAD (Will spawn off track!)");

    if (track->tag == NFS_3 || track->tag == NFS_4) {
        uint32_t nVroad = boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->col.vroadHead.nrec;
        ASSERT(vroadIndex < nVroad, "Requested reset to vroad index: " << vroadIndex << " outside of nVroad: " << nVroad);

        glm::quat rotationMatrix = glm::normalize(glm::quat(glm::vec3(-SIMD_PI / 2, 0, 0)));
        COLVROAD resetVroad = boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->col.vroad[vroadIndex];
        vroadPoint = (rotationMatrix * TrackUtils::pointToVec(resetVroad.refPt)) / 65536.f;
        vroadPoint /= 10.f;
        vroadPoint.y += 0.2;

        // Get VROAD right vector
        glm::vec3 curVroadRightVec = rotationMatrix * glm::vec3(resetVroad.right.x / 128.f, resetVroad.right.y / 128.f, resetVroad.right.z / 128.f);
        vroadPoint += offset * curVroadRightVec;

        rotationMatrix = glm::normalize(glm::quat(glm::vec3(SIMD_PI / 2, 0, 0)));
        glm::vec3 forward = TrackUtils::pointToVec(resetVroad.forward) * rotationMatrix;
        glm::vec3 normal = TrackUtils::pointToVec(resetVroad.normal) * rotationMatrix;
        carOrientation = glm::conjugate(glm::toQuat(
                glm::lookAt(vroadPoint,
                            vroadPoint - forward,
                            normal
                )
        ));
    } else {
        ASSERT(false, "Vroad Index based reset not available outside NFS3 for now.");
    }

    // Go and find the Vroad Data to reset to
    car->resetCar(vroadPoint, carOrientation);
}

bool CarAgent::isWinner() {
    if(droveBack) return false;

    fitness = getClosestVroad(car, track);
    return fitness > 500.f;//boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->col.vroadHead.nrec - 5;
}

void CarAgent::reset(){
    resetToVroad(0, 0.f, track, car);
}

void CarAgent::simulate() {
    if (dead && training){
        return ;
    }

    // If during simulation, car flips, reset
    if((car->upDistance <= 0.1f || car->downDistance > 1.f)){
        resetToVroad(getClosestVroad(car, track), 0.f, track, car);
    }

    std::vector<double> raycastInputs;
    std::vector<double> networkOutputs;

    // Use maximum from front 3 sensors, as per Luigi Cardamone
    float maxForwardDistance = std::max({car->rangefinders[Car::FORWARD_RAY], car->rangefinders[Car::FORWARD_LEFT_RAY], car->rangefinders[Car::FORWARD_RIGHT_RAY]});

    // All inputs roughly between 0 and 5. Speed/10 to bring it into line.
    // -90, -60, -30, maxForwardDistance {-10, 0, 10}, 30, 60, 90, currentSpeed/10.f
    raycastInputs = {car->rangefinders[Car::LEFT_RAY], car->rangefinders[3], car->rangefinders[6], maxForwardDistance, car->rangefinders[12], car->rangefinders[15], car->rangefinders[Car::RIGHT_RAY], car->m_vehicle->getCurrentSpeedKmHour()/ 10.f};
    networkOutputs = {0, 0, 0};

    raceNet.evaluate(raycastInputs, networkOutputs);

    car->applyAccelerationForce(networkOutputs[0] > 0.1f, false);
    car->applyBrakingForce(networkOutputs[1] > 0.1f);
    // Mutex steering
    car->applySteeringLeft(networkOutputs[2] > 0.1f && networkOutputs[3] < 0.1f);
    car->applySteeringRight(networkOutputs[3] > 0.1f && networkOutputs[2] < 0.1f);

    if(!training) return;

    // Speculatively calculate where we're gonna end up
    int new_fitness = getClosestVroad(car, track);

    // If the fitness jumps this much between ticks, we probably reversed over the start line.
    // TODO: Add better logic to prevent this
    if (abs(new_fitness - fitness) > 100){
        dead = droveBack = true;
        return;
    }

    if (new_fitness > fitness){
        tickCount = 0;
        fitness = new_fitness;
    }

    ++tickCount;
    if (tickCount > STALE_TICK_COUNT){
        dead = true;
    }
}

int CarAgent::getClosestVroad(const std::shared_ptr<Car> &car, const std::shared_ptr<ONFSTrack> &track) {
    uint32_t nVroad = boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->col.vroadHead.nrec;

    int closestVroadID = 0;
    float lowestDistance = FLT_MAX;
    for (int vroad_Idx = 0; vroad_Idx < nVroad; ++vroad_Idx) {
        INTPT refPt = boost::get<std::shared_ptr<NFS3_4_DATA::TRACK>>(track->trackData)->col.vroad[vroad_Idx].refPt;
        glm::quat rotationMatrix = glm::normalize(glm::quat(glm::vec3(-SIMD_PI / 2, 0, 0)));
        glm::vec3 vroadPoint = rotationMatrix * glm::vec3((refPt.x / 65536.0f) / 10.f, ((refPt.y / 65536.0f) / 10.f), (refPt.z / 65536.0f) / 10.f);

        float distance = glm::distance(car->rightFrontWheelModel.position, vroadPoint);
        if (distance < lowestDistance) {
            closestVroadID = vroad_Idx;
            lowestDistance = distance;
        }
    }
    // TODO: HACK HACK HACK
    if (closestVroadID == 1363) closestVroadID = 1;

    // Return a number corresponding to the distance driven
    return closestVroadID;
}


