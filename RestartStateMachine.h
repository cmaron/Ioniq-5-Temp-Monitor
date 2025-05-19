#ifndef RESTARTSTATEMACHINE
#define RESTARTSTATEMACHINE

#include "StateMachine.h"
#include "Restarter.h"

// Initial wait time is 5 seconds
#define INITIAL_WAIT_TIME 5000
// Initial backoff coefficient is 2, so we double the wait each time. 
#define DEFAULT_BACKOFF_COEFFICIENT 1.5
// If we hit the maximum wait time we will start over and continue to retry forever.
#define MAX_WAIT_TIME 86400000
// If we have been in an updated state and want to restart, we need this amount of time to have elapsed. 
#define RECONNECT_DELAY 300000

/**
  * The starting state when we fetch the index page (/) and store the frkrouter cookie
  */
void indexStateFunc();

/**
  *  A state where we call init_page.cgi 
  */
void initStateFunc();

/**
  * A state where we post login data to login.cgi
  */
void loginStateFunc();

/**
  * A state where we post updates to the settings and initiate a router restart
  */
void updateStateFunc();

/**
  * A state for sleeping/waiting to try again.
  */
void sleepStateFunc();

bool transitionIndexToInit();
bool transitionInitToLogin();
bool transitionLoginToUpdate();
bool transitionUpdateToSleep();

// All states can transition to sleep after setting successful to false
bool transitionToSleep();
bool staySleeping();
bool tryAgain();

StateMachine fsm = StateMachine();

State* indexState;
State* initState;
State* loginState;
State* updateState;
State* sleepState;
  
void setupStateMachine() {
  indexState = fsm.addState(&indexStateFunc);
  initState = fsm.addState(&initStateFunc);
  loginState = fsm.addState(&loginStateFunc);
  updateState = fsm.addState(&updateStateFunc);
  sleepState = fsm.addState(&sleepStateFunc);

  // Define the state transitions
  indexState->addTransition(&transitionIndexToInit, initState);
  initState->addTransition(&transitionInitToLogin, loginState);
  loginState->addTransition(&transitionLoginToUpdate, updateState);
  updateState->addTransition(&transitionUpdateToSleep, sleepState);

  // All states can move to sleep on failure
  indexState->addTransition(&transitionToSleep, sleepState);
  initState->addTransition(&transitionToSleep, sleepState);
  loginState->addTransition(&transitionToSleep, sleepState);
  updateState->addTransition(&transitionToSleep, sleepState);

  // Sleep is potentially the final "end" state depending on how things go
  sleepState->addTransition(&staySleeping, sleepState);
  sleepState->addTransition(&tryAgain, indexState);
}

class RestartStateMachine {
  public:
    RestartStateMachine() {
      successful = false;

      indexLoaded = false;
      initialized = false;
      loggedIn = false;
      updated = false;

      backoffCoefficient = DEFAULT_BACKOFF_COEFFICIENT;
    }

    void setClient(HttpClient* client) {
      this->client = client;
    }

    String* getFrkrouter() {
      return frkrouter;
    }

    HttpClient* getClient() {
      return client;
    }

    void setFrkrouter(String* value) {
      frkrouter = value;
    }

    bool isSuccessful() {
      return successful;
    }

    bool wasIndexLoaded() {
      return this->indexLoaded;
    }

    bool wasInitialized() {
      return this->initialized;
    }

    bool wasLoggedIn() {
      return this->loggedIn;
    }

    bool wasUpdated() {
      return this->updated;
    }

    void setIndexLoaded() {
      this->indexLoaded = true;
    }

    void setInitialized() {
      this->initialized = true;
    }

    void setLoggedIn() {
      this->loggedIn = true;
    }

    void setUpdated() {
      this->updated = true;
    }

    bool canTryAgain() {
      unsigned long elapsed = millis() - lastSleep;
      Serial.print("Can try again? elapsed=");
      Serial.print(elapsed);
      Serial.print(" waitTime=");
      Serial.println(waitTime);

      if (elapsed >= waitTime) {
        Serial.println("Trying again...");
        return true;
      }
      return false;
    }

    void failed() {
      successful = false;
      indexLoaded = false;
      initialized = false;
      loggedIn = false;
      updated = false;

      lastSleep = millis();
      unsigned long elapsed = lastSleep - lastRestart;
      // If we have reached the updated state, we have to wait for the reconnect delay before trying again
      // Otherwise, we figure out the wait time factoring in the backoffCoefficient
      if (updated && elapsed < RECONNECT_DELAY) {
        waitTime = RECONNECT_DELAY - elapsed;
      } else if (waitTime == 0) {
        waitTime = INITIAL_WAIT_TIME;
      } else {
        waitTime *= backoffCoefficient;
        if (waitTime >= MAX_WAIT_TIME) {
          waitTime = INITIAL_WAIT_TIME;
        }
      }
      Serial.print("Failed! waitTime=");
      Serial.println(waitTime);
    }

    void complete() {
      successful = true;
      lastRestart = millis();
    }

    void restart() {
      if (updated) {
        Serial.println("Restarting...");
        successful = false;

        indexLoaded = false;
        initialized = false;
        loggedIn = false;
        updated = false;

        unsigned long elapsed = millis() - lastRestart;
        if (elapsed < RECONNECT_DELAY) {
          waitTime = RECONNECT_DELAY - elapsed;
        }
        lastSleep = millis();
      }
    }

  private:
    bool successful; // If true the FSM has reached a successful conclusion and will 
                     // not transition from sleep unless this is set to false externally. 

    double waitTime; // How long to wait for the next attempt to restart the router. 
    double backoffCoefficient; // If we are in an error state and need to retry we must wait

    // Other state variables, like cookies, ids, timing, etc
    String* frkrouter;
    unsigned long lastSleep; // The last time a restart was... started 
    unsigned long lastRestart; // When we last successfully restarted (if we did)

    HttpClient* client;

    bool indexLoaded;
    bool initialized;
    bool loggedIn;
    bool updated;
};

RestartStateMachine stateMachine;

// ##########
// # STATES #
// ##########
/**
  * The starting state when we fetch the index page (/) and store the frkrouter cookie
  */
void indexStateFunc() {
  if (fsm.executeOnce) {
    Serial.println("Processing Index\n");
    String* cookie = processIndexPage(stateMachine.getClient());
    stateMachine.setFrkrouter(cookie);
    if (cookie != NULL) {
      Serial.println("\nIndex loaded");
      stateMachine.setIndexLoaded();
    }
  }
}

/**
  *  A state where we call init_page.cgi 
  */
void initStateFunc() {
  if (fsm.executeOnce) {
    Serial.println("Processing Init");
    if(processInitPage(stateMachine.getClient(), stateMachine.getFrkrouter())) {
      Serial.println("Initialized");
      stateMachine.setInitialized();
    }
  }
}

/**
  * A state where we post login data to login.cgi
  */
void loginStateFunc() {
  if (fsm.executeOnce) {
    Serial.println("Processing Login");
    if(processLoginPage(stateMachine.getClient(), stateMachine.getFrkrouter())) {
      Serial.println("Logged In");
      stateMachine.setLoggedIn();
    }
  }
}

/**
  * A state where we post updates to the settings and initiate a router restart
  */
void updateStateFunc() {
  if (fsm.executeOnce) {
    Serial.println("Processing Settings Update");
    if(processSettingsUpdate(stateMachine.getClient(), stateMachine.getFrkrouter())) {
      Serial.println("Updated");
      stateMachine.setUpdated();
    }
  }
}

/**
  * A state for sleeping/waiting to try again.
  */
void sleepStateFunc() {
  Serial.println("Sleeping");
}

// ###############
// # TRANSITIONS #
// ###############

bool transitionIndexToInit() {
  Serial.print("Index -> Init: ");
  Serial.println(stateMachine.wasIndexLoaded());
  return stateMachine.wasIndexLoaded();
}

bool transitionInitToLogin() {
  Serial.print("Init -> Login: ");
  Serial.println(stateMachine.wasInitialized());
  return stateMachine.wasInitialized();
}

bool transitionLoginToUpdate() {
  Serial.print("Login -> Update: ");
  Serial.println(stateMachine.wasLoggedIn());
  return stateMachine.wasLoggedIn();
}

bool transitionUpdateToSleep() {
  // If everything works out, mark us as successful
  if (stateMachine.wasUpdated()) {
      Serial.println("UPDATED!");
      stateMachine.complete();
      return true;
  }
  return false;
}

// All states can transition to sleep after setting successful to false

bool transitionToSleep() {
  Serial.println("Error! Sleepy time");
  stateMachine.failed();
  return true;
}

bool staySleeping() {
  Serial.println("Sleeping...");
  return stateMachine.isSuccessful();
}

bool tryAgain() {
  // This should factor in how long it's been since we last tried and use that to determine what to do...
  Serial.println("Try again?");
  return stateMachine.canTryAgain();
}

#endif