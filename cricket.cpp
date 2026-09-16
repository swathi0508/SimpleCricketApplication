#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <vector>
#include <memory>

enum State
{
    AWAITING_BOWLER = 1,
    AWAITING_BATTER = 2,
    AWAITING_UMPIRE = 3,
    GAME_OVER = 4
};

struct MatchVerdict
{
    int runsToAward;
    bool isValidBall;
    bool isWicket;
    bool isRunOut;
    bool isStrikerOut;
};

class Player
{
    private:
    std::string playerName;

    int runsScored;
    int ballsFaced;
    bool isOut;

    int ballsBowled;
    int wicketsTaken;

    public:
    Player(std::string name) : playerName(name)
    {
        runsScored = 0;
        ballsFaced = 0;
        isOut = false;
        ballsBowled = 0;
        wicketsTaken = 0;
    }

    ~Player() {}

    std::string getPlayerName()
    {
        return this->playerName;
    }

    void addRuns(int runsScored)
    {
        this->runsScored += runsScored;
    }

    void incrementBallsFaced()
    {
        this->ballsFaced++;
    }

    void setIsOut()
    {
        this->isOut = true;
    }

    void incrementBallsBowled()
    {
        this->ballsBowled++;
    }

    void addWicket()
    {
        this->wicketsTaken++;
    }

    void displayPlayerStats()
    {
        std::cout << "Player Name: " << this->playerName << std::endl;
        std::cout << "Total runs scored: " << this->runsScored << std::endl;
        std::cout << "Total balls faced: " << this->ballsFaced << std::endl;
        std::cout << "Was out: " << (this->isOut ? "Yes" : "No") << std::endl;
        std::cout << "Total balls bowled: " << this->ballsBowled << std::endl;
        std::cout << "Wickets taken: " << this->wicketsTaken << std::endl << std::endl;
    }
};

class Team
{
    private:
    std::string teamName;
    std::vector<std::shared_ptr<Player>> players;
    int nextBatterIndex;
    int nextBowlerIndex;

    public:
    Team(std::string name, std::vector<std::string> playerNames) : teamName(name)
    {
        players.reserve(11);
        for(std::string name : playerNames)
        {
            players.push_back(std::make_shared<Player>(name));
        }
        nextBatterIndex = 2; //Two batters in the field already
        nextBowlerIndex = 0;
    }

    ~Team(){}

    std::shared_ptr<Player> getNextBatter()
    {
        if(nextBatterIndex < 11)
        {
            return players.at(nextBatterIndex++);
        }
        return nullptr;
    }

    std::shared_ptr<Player> getNextBowler()
    {
        nextBowlerIndex = (nextBowlerIndex + 1) % 11;
        return players.at(nextBowlerIndex);
    }

    std::shared_ptr<Player> getPlayerByIndex(int index)
    {
        return players.at(index);
    }

    std::string getTeamName()
    {
        return teamName;
    }

    void resetBatterBowlerIdx()
    {
        nextBatterIndex = 2;
        nextBowlerIndex = 0;
    }

};

class ScoreBoard
{
    private:
    std::weak_ptr<Player> striker;
    std::weak_ptr<Player> nonStriker;
    std::weak_ptr<Player> currentBowler;

    int totalRuns;
    int totalWickets;
    int totalValidBalls;

    State currentState;
    int latestBowlerRoll;
    int latestBatterRoll;
    bool isRollOut;

    void handleNormalRuns(int runs)
    {
        totalRuns += runs;
        totalValidBalls++;

        if(std::shared_ptr<Player> striker_tempShared = striker.lock())
        {
            striker_tempShared->addRuns(runs);
            striker_tempShared->incrementBallsFaced();
            std::cout << "[UMPIRE]: " << striker_tempShared->getPlayerName() << " hits the ball and scores: " << runs << " runs!" << std::endl;
        }

        if(std::shared_ptr<Player> currentBowler_tempShared = currentBowler.lock())
        {
            currentBowler_tempShared->incrementBallsBowled();
        }
    }

    void handleExtraRun()
    {
        totalRuns++;
        std::cout << "[UMPIRE] Penalty! Wide Ball. +1 Extra Run to team!" << std::endl;
    }

    void handleWicket(const MatchVerdict& verdict)
    {
        totalWickets++;
        totalValidBalls++;
        totalRuns += verdict.runsToAward;

        if(std::shared_ptr<Player> currentBowler_tempShared = currentBowler.lock())
        {
            currentBowler_tempShared->incrementBallsBowled();
            if(!verdict.isRunOut)
            {
                currentBowler_tempShared->addWicket();
            }
        }

        if(std::shared_ptr<Player> striker_tempShared = striker.lock())
        {
            striker_tempShared->incrementBallsFaced();
            striker_tempShared->addRuns(verdict.runsToAward);
        }

        if(!verdict.isRunOut)
        {
            if(std::shared_ptr<Player> striker_tempShared = striker.lock())
            {
                striker_tempShared->setIsOut();
                std::cout << "[UMPIRE]: Out! " << striker_tempShared->getPlayerName() << " is moved out!" << std::endl;
            }
        }
        else
        {
            if(verdict.isStrikerOut)
            {
                if(std::shared_ptr<Player> striker_tempShared = striker.lock())
                {
                    striker_tempShared->setIsOut();
                    std::cout << "[UMPIRE]: Run Out! " << striker_tempShared->getPlayerName() << " is moved out!" << std::endl;
                }
            }
            else
            {
                if(std::shared_ptr<Player> nonStriker_tempShared = nonStriker.lock())
                {
                    nonStriker_tempShared->setIsOut();
                    std::cout << "[UMPIRE]: Run Out! " << nonStriker_tempShared->getPlayerName() << " is moved out!" << std::endl;
                }
            }
        }
    }

    public:
    ScoreBoard(std::shared_ptr<Player> striker, std::shared_ptr<Player> nonStriker, std::shared_ptr<Player> currentBowler)
        : striker(striker), nonStriker(nonStriker), currentBowler(currentBowler)
    {
        totalRuns = 0;
        totalWickets = 0;
        totalValidBalls = 0;

        currentState = State::AWAITING_BOWLER;
        latestBowlerRoll = 0;
        latestBatterRoll = 0;
    }

    ~ScoreBoard() {}

    State getCurrentState()
    {
        return this->currentState;
    }

    void setCurrentState(State newState)
    {
        this->currentState = newState;
    }

    int getBowlerRoll()
    {
        return this->latestBowlerRoll;
    }

    void setBowlerRoll(int roll)
    {
        this->latestBowlerRoll = roll;
    }

    int getBatterRoll()
    {
        return this->latestBatterRoll;
    }

    void setBatterRoll(int roll)
    {
        this->latestBatterRoll = roll;
    }

    bool getRollOut()
    {
        return isRollOut;
    }

    void setRollOut(bool isRollOut)
    {
        this->isRollOut = isRollOut;
    }

    int getTotalRuns()
    {
        return this->totalRuns;
    }

    void swapBatsmen()
    {
        std::swap(this->striker, this->nonStriker);
    }

    void applyVerdict(const MatchVerdict& verdict)
    {
        if(!verdict.isValidBall)
        {
            handleExtraRun();
        }
        else if(verdict.isWicket)
        {
            handleWicket(verdict);
        }
        else
        {
            handleNormalRuns(verdict.runsToAward);
        }
    }

    void replaceStriker(std::shared_ptr<Player> newBatter)
    {
        this->striker = newBatter;
    }

    void replaceNonStriker(std::shared_ptr<Player> newBatter)
    {
        this->nonStriker = newBatter;
    }

    bool isMatchOver()
    {
        return ((totalWickets >= 10) || (totalValidBalls >= 6));
    }

    void displayInitialScoreBoard(const int &innings, const std::string battingTeam, const std::string bowlingTeam)
    {
        std::cout << std::endl << "--------------------------------------------------------" << std::endl;
        std::cout << "Innings: " << innings << " | Batting Team: " << battingTeam << " | Bowling Team: " << bowlingTeam << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;
        std::cout << "Over " << (totalValidBalls < 6 ? ("0." + std::to_string(totalValidBalls)) : "1.0") << std::endl;
        std::cout << "Total Runs: " << totalRuns << std::endl;
        if(std::shared_ptr<Player> striker_tempShared = striker.lock())
        {
            std::cout << "Striker: " << striker_tempShared->getPlayerName() << std::endl;
        }
        
        if(std::shared_ptr<Player> nonStriker_tempShared = nonStriker.lock())
        {
            std::cout << "Non-Striker: " << nonStriker_tempShared->getPlayerName() << std::endl;
        }
        if(std::shared_ptr<Player> currentBowler_tempShared = currentBowler.lock())
        {
            std::cout << "Bowler: " << currentBowler_tempShared->getPlayerName() << std::endl;
        }
        std::cout << "--------------------------------------------------------" << std::endl << std::endl;
    }

    void displayScoreBoard(const MatchVerdict &verdict)
    {
        std::cout << std::endl << "--------------------------------------------------------" << std::endl;
        std::cout << "Latest Score Board..." << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;
        std::cout << "Overs: " << (totalValidBalls < 6 ? ("0." + std::to_string(totalValidBalls)) : "1.0") << " finished!" << std::endl;
        std::cout << "Latest Run: " << verdict.runsToAward << std::endl;
        std::cout << "Total Runs: " << totalRuns << std::endl;
        if(std::shared_ptr<Player> striker_tempShared = striker.lock())
        {
            std::cout << "Striker: " << striker_tempShared->getPlayerName() << std::endl;
        }
        
        if(std::shared_ptr<Player> nonStriker_tempShared = nonStriker.lock())
        {
            std::cout << "Non-Striker: " << nonStriker_tempShared->getPlayerName() << std::endl;
        }
        if(std::shared_ptr<Player> currentBowler_tempShared = currentBowler.lock())
        {
            std::cout << "Bowler: " << currentBowler_tempShared->getPlayerName() << std::endl;
        }
        std::cout << "--------------------------------------------------------" << std::endl << std::endl;
    }
};

class Umpire
{
    public:
    Umpire(){}
    ~Umpire(){}

    MatchVerdict evaluateBall(int bowlerRoll, int batterRoll, bool randomOutRoll)
    {
        MatchVerdict verdict;
        if(bowlerRoll == 0) //wide = penalty
        {
            verdict.runsToAward = 1;
            verdict.isValidBall = false;
            verdict.isWicket = false;
            verdict.isRunOut = false;
            verdict.isStrikerOut = false;
        }
        else if(randomOutRoll)
        {
            verdict.isValidBall = true;
            verdict.isWicket = true;

            if(batterRoll == 0) //Bowled/caught
            {
                verdict.runsToAward = 0;
                verdict.isRunOut = false;
                verdict.isStrikerOut = true;
            }
            else //run-out
            {
                verdict.runsToAward = batterRoll - 1;
                verdict.isRunOut = true;
                verdict.isStrikerOut = std::rand() % 2; //Randomly select striker or non-striker to be out
            }
        }
        else //normal runs
        {
            verdict.runsToAward = batterRoll;
            verdict.isValidBall = true;
            verdict.isWicket = false;
            verdict.isRunOut = false;
            verdict.isStrikerOut = false;
        }
        return verdict;
    };
};

void bowlerThread(ScoreBoard &scoreBoard, std::mutex &mtx, std::condition_variable &cv)
{
    while(true)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&scoreBoard](){
            return ((scoreBoard.getCurrentState() == State::AWAITING_BOWLER)
            || (scoreBoard.getCurrentState() == State::GAME_OVER)
            || (scoreBoard.isMatchOver()));
        });

        if((scoreBoard.getCurrentState() == State::GAME_OVER) || scoreBoard.isMatchOver())
        {
            break;
        }

        int bowlerRoll = std::rand() % 10; //generate bowler roll between 0-9 where 0=Wide
        scoreBoard.setBowlerRoll(bowlerRoll);

        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << "[UMPIRE]: Bowler delivers the ball... Roll: " << bowlerRoll << std::endl;
        
        scoreBoard.setCurrentState(State::AWAITING_BATTER);
        cv.notify_all();
    }
}

void batterThread(ScoreBoard &scoreBoard, std::mutex &mtx, std::condition_variable &cv)
{
    while(true)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&scoreBoard](){
            return ((scoreBoard.getCurrentState() == State::AWAITING_BATTER)
                || (scoreBoard.getCurrentState() == State::GAME_OVER)
                || (scoreBoard.isMatchOver()));
        });

        if((scoreBoard.getCurrentState() == State::GAME_OVER) || scoreBoard.isMatchOver())
        {
            break;
        }
        
        int batterRoll = std::rand() % 7; //generating run score between 0-6
        scoreBoard.setBatterRoll(batterRoll);

        bool isRunOut = ((std::rand() % 100) < 30); //generating random 30% chance of a run-out
        scoreBoard.setRollOut(isRunOut);

        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << "[UMPIRE]: Batter Rolls: " << batterRoll << " , RunOut: " << (isRunOut ? "yes" : "no") << std::endl;

        scoreBoard.setCurrentState(State::AWAITING_UMPIRE);
        cv.notify_all();
    }
}

int main()
{
    std::vector<std::string> teamA = {"A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10", "A11"};
    std::vector<std::string> teamB = {"B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "B10", "B11"};

    Team currentBattingTeam("TeamA", teamA);
    Team currentBowlingTeam("TeamB", teamB);

    int teamAScore = 0;
    int teamBScore = 0;
    int targetScore = -1;

    std::srand(std::time(nullptr));

    for(int currentInnings = 1; currentInnings <= 2; currentInnings++)
    {
        std::shared_ptr<Player> striker = currentBattingTeam.getPlayerByIndex(0);
        std::shared_ptr<Player> nonStriker = currentBattingTeam.getPlayerByIndex(1);
        std::shared_ptr<Player> bowler = currentBowlingTeam.getPlayerByIndex(0);

        ScoreBoard scoreBoard(striker, nonStriker, bowler);
        Umpire umpire;

        scoreBoard.displayInitialScoreBoard(currentInnings, currentBattingTeam.getTeamName(), currentBowlingTeam.getTeamName());

        std::mutex mtx;
        std::condition_variable cv;

        std::thread bowler_thread(bowlerThread, std::ref(scoreBoard), std::ref(mtx), std::ref(cv));
        std::thread batter_thread(batterThread, std::ref(scoreBoard), std::ref(mtx), std::ref(cv));

        while(true)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&scoreBoard](){
                return ((scoreBoard.getCurrentState() == State::AWAITING_UMPIRE)
                    || (scoreBoard.getCurrentState() == State::GAME_OVER)
                    || (scoreBoard.isMatchOver()));
            });

            if((scoreBoard.getCurrentState() == State::GAME_OVER) || scoreBoard.isMatchOver())
            {
                break;
            }

            MatchVerdict verdict = umpire.evaluateBall(scoreBoard.getBowlerRoll(), scoreBoard.getBatterRoll(), scoreBoard.getRollOut());
            scoreBoard.applyVerdict(verdict);

            if(verdict.isWicket)
            {
                std::shared_ptr<Player> newBatter = currentBattingTeam.getNextBatter();
                if(newBatter != nullptr)
                {
                    if(verdict.isStrikerOut)
                    {
                        scoreBoard.replaceStriker(newBatter);
                    }
                    else
                    {
                        scoreBoard.replaceNonStriker(newBatter);
                    }
                }
                else
                {
                    std::cout << "[UMPIRE]: Out of players in " << currentBattingTeam.getTeamName() << std::endl;
                    break;
                }
            }

            if((verdict.runsToAward %2 != 0) && verdict.isValidBall)
            {
                scoreBoard.swapBatsmen();
            }

            scoreBoard.displayScoreBoard(verdict);

            if(targetScore > 0 && scoreBoard.getTotalRuns() >= targetScore)
            {
                std::cout << "[UMPIRE]: " << currentBattingTeam.getTeamName() << " has chased the target score and won the match!!" << std::endl;
                break;
            }

            if(scoreBoard.isMatchOver())
            {
                scoreBoard.setCurrentState(State::GAME_OVER);
                break;
            }
            else
            {
                scoreBoard.setCurrentState(State::AWAITING_BOWLER);
            }

            cv.notify_all();
        }

        {
            std::lock_guard<std::mutex> lock(mtx);
            scoreBoard.setCurrentState(State::GAME_OVER);
        }
        cv.notify_all();
        bowler_thread.join();
        batter_thread.join();

        std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << std::endl;
        if(currentInnings == 1)
        {
            teamAScore = scoreBoard.getTotalRuns();
            targetScore =  teamAScore + 1;

            std::cout << "[UMPIRE]: Innings 1 Over! " << currentBattingTeam.getTeamName() << " scored " << teamAScore << " runs." << std::endl;
            std::cout << "[UMPIRE]: Target for " << currentBowlingTeam.getTeamName() << " is " << targetScore << " runs."  << std::endl;

            std::swap(currentBattingTeam, currentBowlingTeam);
            currentBattingTeam.resetBatterBowlerIdx();
            currentBowlingTeam.resetBatterBowlerIdx();
        }
        else
        {
            teamBScore = scoreBoard.getTotalRuns();
            std::cout << "[UMPIRE]: Innings 2 Over! " << currentBattingTeam.getTeamName() << " scored " << teamBScore << " runs." << std::endl;
        }
        std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << std::endl << std::endl;
    }

    std::cout << "========================================================" << std::endl;
    std::cout << "                     MATCH RESULT                       " << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << "Team A Score: " << teamAScore << std::endl;
    std::cout << "Team B Score: " << teamBScore << std::endl;

    if (teamBScore > teamAScore)
    {
        std::cout << "[UMPIRE]: Team B wins by chasing the target of: " << targetScore << "!" << std::endl;
    }
    else if (teamAScore > teamBScore)
    {
        std::cout << "[UMPIRE]: Team A wins by " << (teamAScore - teamBScore) << " runs!" << std::endl;
    }
    else
    {
        std::cout << "[UMPIRE]: The match ended in a TIE!" << std::endl;
    }
    std::cout << "========================================================" << std::endl;

    return 0;
}
