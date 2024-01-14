#include "pch.h"
#include "AirOrientationTrainer.h"
#include "bakkesmod/wrappers/wrapperstructs.h"
#include "random"

BAKKESMOD_PLUGIN(AirOrientationTrainer, "Air Orientation Trainer", plugin_version, PLUGINTYPE_FREEPLAY)

// initiate variables
std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
bool AOTEnabled, lockTraining, tickFlipFlop, newPB, loadedPBs, replaySlowRound = false;
bool firstNewTrain = true;
int initCarPitch, initCarYaw, initCarRoll, targetPitch, targetYaw, targetRoll, trainingMode, trainingCount, trainingCurrent, elapsedTime;
int roundTimerLimit = 5;
float completeTime, lastRoundTime;
std::vector<int> bestTimes = { 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080 };

void AirOrientationTrainer::onLoad()
{
	_globalCvarManager = cvarManager;
	LOG("AOT Plugin loaded!");
	
	cvarManager->registerNotifier("airOrientationTrainer", [this](std::vector<std::string> args) {
		trainingCount = trainingCurrent = 0;
		firstNewTrain = true;
		reset();
		}, "", PERMISSION_ALL);
	cvarManager->registerNotifier("AOT_disable", [this](std::vector<std::string> args) {
		onUnload();
		}, "", PERMISSION_ALL);

	cvarManager->registerCvar("AOT_enabled", "0", "Enable AOT", true, true, 0, true, 1, false)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		AOTEnabled = cvar.getBoolValue();
		});
	cvarManager->registerCvar("AOT_training_mode", "Default Pack", "Select training mode")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		trainingMode = cvar.getIntValue();
	});
	cvarManager->registerCvar("AOT_difficulty", "14", "Difficulty", true, true, 5, true, 25);

	cvarManager->registerCvar("AOT_best_times", "1080,1080,1080,1080,1080,1080,1080,1080,1080,1080", "High scores", false);
	
	cvarManager->registerCvar("AOT_round_time_limit", "5", "Replay round time limit", true, true, 1, true, 8)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		roundTimerLimit = cvar.getIntValue();
			});

	cvarManager->registerCvar("AOT_lock_training", "0", "Replay current orientation", true, true, 0, true, 1, false)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		lockTraining = cvar.getBoolValue();
			});
}
void AirOrientationTrainer::onUnload() {
	LOG("AOT Plugin stopped!");
	if (!gameWrapper->IsInFreeplay()) { return; }
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) { return; }

	// reset gravity
	cvarManager->getCvar("sv_soccar_gravity").setValue("-650");
}
void AirOrientationTrainer::reset() {
	if (!gameWrapper->IsInFreeplay()) { return; }
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) { return; }

	if (!AOTEnabled) { return; }

	CarWrapper car = gameWrapper->GetLocalCar();
	if (!car) { return; }
	BallWrapper ball = server.GetBall();
	if (!ball) { return; }

	// load best times
	if (!loadedPBs) {
		CVarWrapper bestTimesCVar = cvarManager->getCvar("AOT_best_times");
		if (!bestTimesCVar) { return; }
		std::string str = bestTimesCVar.getStringValue();
		std::vector<std::string> vecOfStrings;
		char* token = strtok(const_cast<char*>(str.c_str()), ",");
		while (token != nullptr)
		{
			vecOfStrings.push_back(std::string(token));
			token = strtok(nullptr, ",");
		}
		for (int i = 0; i < vecOfStrings.size(); i++) {
			bestTimes.at(i) = stoi(vecOfStrings.at(i));
		};
		loadedPBs = true;
	}

	// default training or random orientations (skip if locked to current orientation or over the set time limit)
	if (firstNewTrain || !lockTraining && elapsedTime < roundTimerLimit * 120) {
		if (trainingMode == 0) {
			GetTraining();
		}
		else if (trainingMode == 1) {
			std::random_device rd;
			std::uniform_int_distribution<int> dist(-32767, 32767);
			std::uniform_int_distribution<int> dist2(-16383, 16383);
			initCarPitch = dist2(rd);
			initCarYaw = dist(rd);
			initCarRoll = dist(rd);
			targetPitch = dist2(rd);
			targetYaw = -abs(dist(rd)); // only yaw pointing towards blue goal, less confusing and still allows all possible rotations
			targetRoll = dist(rd);
			firstNewTrain = false;
		}
	}

	// setup the game
	ball.SetVelocity(Vector{ 900, 400, -300 });
	ball.SetLocation(Vector{ -2000, -200, 1200 });
	car.SetLocation(Vector{ 0, 2000, 1000 });
	car.SetVelocity(Vector{ 0, 0, 0 });
	car.SetAngularVelocity(Vector{ 0, 0, 0 },0);
	car.SetCarRotation(Rotator{ initCarPitch,  initCarYaw, initCarRoll });
	car.SetbDoubleJumped(true);
	cvarManager->getCvar("sv_soccar_gravity").setValue("-0.0000001");

	// check if round was too slow, if so render message and time
	(elapsedTime < roundTimerLimit * 120) ? replaySlowRound = false : replaySlowRound = true;

	// reset round timers
	lastRoundTime = (float)elapsedTime / 120;
	elapsedTime = 0;

	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		Render(canvas);
		});
}
void AirOrientationTrainer::Render(CanvasWrapper canvas)
{
	if (!gameWrapper->IsInFreeplay()) { return; }
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) { return; }
	
	if (!AOTEnabled) { return; }

	CarWrapper car = gameWrapper->GetLocalCar();
	if (!car) { return; }
	
	Rotator carRotation = car.GetRotation();
	car.SetLocation(Vector{ 0, 2000, 1000 });
	car.SetVelocity(Vector{ 0, 0, 0 });

	elapsedTime++; // increment round timer
	
	// calculate target 3D points to render
	Quat targetQuat = RotatorToQuat(Rotator{ targetPitch,  targetYaw, targetRoll });
	Vector frontCarVec = RotateVectorWithQuat(Vector{ 80, 0, 0 }, targetQuat) + Vector{ 0, 2000, 1000 };
	Vector leftCarVec = RotateVectorWithQuat(Vector{ -20, 20, 0 }, targetQuat) + Vector{ 0, 2000, 1000 };
	Vector rightCarVec = RotateVectorWithQuat(Vector{ -20, -20, 0 }, targetQuat) + Vector{ 0, 2000, 1000 };
	Vector targetRollVector = RotateVectorWithQuat(Vector{ 0, 0, -30 }, targetQuat) + frontCarVec;
	Vector rollArrow1 = RotateVectorWithQuat(Vector{ 0, 5, 0 }, targetQuat) + frontCarVec;
	Vector rollArrow2 = RotateVectorWithQuat(Vector{ 0, -5, 0 }, targetQuat) + frontCarVec;
	
	// calculate actual car vectors and angle to target
	Quat carQuat = RotatorToQuat(car.GetRotation());
	Vector actualForwardDirection = RotateVectorWithQuat(Vector{ 1, 0, 0 }, carQuat);
	Vector actualBelowDirection = RotateVectorWithQuat(Vector{ 0, 0, -1 }, carQuat);
	Vector targetForwardDirection = RotateVectorWithQuat(Vector{ 1, 0, 0 }, targetQuat);
	Vector targetBelowDirection = RotateVectorWithQuat(Vector{ 0, 0, -1 }, targetQuat);
	float forwardAngle = std::acos(actualForwardDirection.X * targetForwardDirection.X + actualForwardDirection.Y * targetForwardDirection.Y + actualForwardDirection.Z * targetForwardDirection.Z);
	float belowAngle = std::acos(actualBelowDirection.X * targetBelowDirection.X + actualBelowDirection.Y * targetBelowDirection.Y + actualBelowDirection.Z * targetBelowDirection.Z);

	// difficulty 10 means angle to target needs to be within 10%. Lower = harder
	CVarWrapper difficultyCVar = cvarManager->getCvar("AOT_difficulty");
	if (!difficultyCVar) { return; }
	int difficulty = difficultyCVar.getIntValue();

	// go next if target is achieved
	float rot_accuracy = difficulty * std::_Pi_val / 100;
	(tickFlipFlop == true) ? tickFlipFlop = false : tickFlipFlop = true; // only reset every 2nd tick to avoid edge cases
	if (forwardAngle < rot_accuracy && tickFlipFlop) {
		if (belowAngle < rot_accuracy) {
			if (trainingMode == 0) { // if new best time in default training, update score and save to file
				newPB = false;
				if (bestTimes.at(trainingCurrent) > elapsedTime) {
					bestTimes.at(trainingCurrent) = elapsedTime;
					newPB = true;
					std::ostringstream oss;
					std::copy(std::begin(bestTimes), std::end(bestTimes), std::ostream_iterator<int>(oss, ","));
					std::string str(oss.str());
					CVarWrapper bestTimesCVar = cvarManager->getCvar("AOT_best_times");
					if (!bestTimesCVar) { return; }
					bestTimesCVar.setValue(str);
					cvarManager->executeCommand("writeconfig", false); // false just means don't log the writeconfig
				}
			}
			reset();
		}
	}

	// calculate 2D projection points from 3D points
	Vector2 centerProj = canvas.Project(Vector{ 0, 2000, 1000 });
	Vector2 startProj = canvas.Project(frontCarVec);
	Vector2 leftCarProj = canvas.Project(leftCarVec);
	Vector2 rightCarProj = canvas.Project(rightCarVec);
	Vector2 targetRollProj = canvas.Project(targetRollVector);
	Vector2 rollArr1Proj = canvas.Project(rollArrow1);
	Vector2 rollArr2Proj = canvas.Project(rollArrow2);
	
	// render target lines
	canvas.SetColor(255, 255, 0, 255);
	canvas.DrawLine(startProj, centerProj);
	canvas.DrawLine(startProj, leftCarProj);
	canvas.DrawLine(startProj, rightCarProj);
	int green;
	(targetRollVector.Y - frontCarVec.Y < 0) ? green = 64 : green = 128; // different orange if arrow pointing backwards, helps to understand arrow directions
	canvas.SetColor(255, green, 0, 255);
	canvas.DrawLine(targetRollProj, startProj);
	canvas.DrawLine(targetRollProj, rollArr1Proj);
	canvas.DrawLine(targetRollProj, rollArr2Proj);
	
	// tutorial text. Only show until one of the first few levels is completed in under 5s
	if (bestTimes.at(0) > 600 && bestTimes.at(1) > 600 && bestTimes.at(2) > 600){
		canvas.SetColor(255, 255, 0, 255);
		canvas.SetPosition(Vector2F{ 150.0, 220.0 });
		canvas.DrawString("Align the front of your car with the yellow arrow, wheels toward orange.", 2.0, 2.0, false);
	}
	
	// uncomment to show car angle to target % numbers on screen
	/*std::string fa = std::to_string(forwardAngle / std::_Pi_val * 100);
	std::string ba = std::to_string(belowAngle / std::_Pi_val * 100);
	fa.erase(fa.length() - 7);
	ba.erase(ba.length() - 7);
	canvas.SetColor(255, 255, 0, 255);
	canvas.SetPosition(Vector2F{ 1.0, 590.0 });
	canvas.DrawString("Front car angle: " + fa + "%", 1.0, 1.0, false);
	canvas.SetPosition(Vector2F{ 1.0, 610.0 });
	canvas.DrawString("Wheel angle: " + ba + "%", 1.0, 1.0, false);*/

	// uncomment to show car rotation numbers on screen
	/*canvas.SetColor(255, 255, 0, 255);
	canvas.SetPosition(Vector2F{ 1.0, 660.0 });
	canvas.DrawString("Pitch: " + std::to_string(carRotation.Pitch), 1.0, 1.0, false);
	canvas.SetPosition(Vector2F{ 1.0, 680.0 });
	canvas.DrawString("Yaw  : " + std::to_string(carRotation.Yaw), 1.0, 1.0, false);
	canvas.SetPosition(Vector2F{ 1.0, 700.0 });
	canvas.DrawString("Roll: " + std::to_string(carRotation.Roll), 1.0, 1.0, false);*/

	if (elapsedTime > 240) { newPB = false; } // display "NEW PB!" onscreen for a maximum of 2s
	if (elapsedTime > 360) { replaySlowRound = false; } // display "replay round" onscreen for a maximum of 3s
	// display if round time limit was not beaten
	if (replaySlowRound) {
		std::string y = std::to_string(lastRoundTime);
		y.erase(y.length() - 4);
		canvas.SetColor(255, 140, 0, 255);
		canvas.SetPosition(Vector2F{ 500.0, 50.0 });
		canvas.DrawString("Replaying round, your time was " + y + "s", 3.0, 3.0, false);
	}
	// build scoreboard
	if (trainingMode == 0) {
		canvas.SetColor(255, 80, 0, 255);
		canvas.SetPosition(Vector2F{ 1.0, 260.0 });
		canvas.DrawString("Best Times", 2.0, 2.0, false);
		canvas.SetColor(255, 255, 0, 255);
		for (int i = 0; i < 10; i++) {
			float verticalPos = i * 20 + 300;
			float x = bestTimes.at(i);
			x = std::round(x / 120 * 1000)/1000;
			std::string y = std::to_string(x);
			y.erase(y.length() - 4);
			std::string currentFlag = (i == trainingCurrent) ? "--" : "";
			canvas.SetPosition(Vector2F{ 1.0, verticalPos });
			canvas.DrawString(currentFlag + "Orientation " + std::to_string(i + 1) + ": " + y, 1.0, 1.0, false);
		}
		if (newPB) {
			canvas.SetColor(50, 160, 0, 255);
			canvas.SetPosition(Vector2F{ 1.0, 490.0 });
			canvas.DrawString("New Personal Best!", 5.0, 5.0, false);
		}
	}
}
void AirOrientationTrainer::GetTraining() {
	std::vector<Rotator> initRotPack = {
		Rotator({ 0,-16384,-16384}),
		Rotator({ 9850,-2798,29250}),
		Rotator({ -8323,-1127,-11193}),
		Rotator({ 14951,-31386,21188}),
		Rotator({ -3696,-29980,-3299}),
		Rotator({ -10202,-16691,30548}),
		Rotator({ -1338,-3046,-13389}),
		Rotator({ -1980,13516,32344}),
		Rotator({ 3064,23786,-18519}),
		Rotator({ 447,-4769,15907})
		
	};
	std::vector<Rotator> targRotPack = {
		Rotator({ 5072,-16384,0 }),
		Rotator({ -7175,-694,15673}),
		Rotator({ -6488,-17583,18034}),
		Rotator({ 5879,-13701,24083}),
		Rotator({ -9689,-7394,28096 }),
		Rotator({ 3091,-11642,5481}),
		Rotator({ -6211,-24238,-23391}),
		Rotator({ -30,-19410,18518}),
		Rotator({ 13609,-4556,-24527}),
		Rotator({ 1923,-29545,-25945})
	};

	Rotator initRot = initRotPack.at(trainingCount);
	Rotator targRot = targRotPack.at(trainingCount);

	initCarPitch = initRot.Pitch;
	initCarYaw = initRot.Yaw;
	initCarRoll = initRot.Roll;
	targetPitch = targRot.Pitch;
	targetYaw = targRot.Yaw;
	targetRoll = targRot.Roll;

	firstNewTrain = false;
	trainingCurrent = trainingCount;
	trainingCount++;

	// Upon training pack completion, loop back to start
	if (trainingCount >= initRotPack.size()) {
		trainingCount = 0;
	}
}