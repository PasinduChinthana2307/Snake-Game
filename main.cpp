#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <EEPROM.h>

#define TFT_DC 9
#define TFT_CS 10
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define SNAKE_COLOR 0x07E0   // Green color for snake
#define BACKGROUND_COLOR 0x0000 // Black for background
#define NORMAL_FOOD_COLOR 0xFFFF // WHITE for noraml food color
#define RED_FOOD 0xFB00

#define VERT_PIN A0 // define parameters for joystick
#define HORZ_PIN A1
#define SELECT 7

#define RIGHT 0b0001 // define the directions in binary 
#define LEFT 0b0010
#define DOWN 0b0011
#define UP 0b0100

#define MAX_RED_FOOD 5

#define BUZZER 6
int melody[] = {523, 494, 440, 392, 349, 329, 294,262, 330, 392, 523};

struct point{
  int16_t x; // x coordinate of the snake's particular rectangle 
  int16_t y; // y coordinate of the snake's particular rectangle
};

struct barrier9 {
  int16_t x;      // X-coordinate of top-left corner
  int16_t y;      // Y-coordinate of top-left corner
  int8_t width;  // Width of the barrier
  int8_t height; // Height of the barrier
};

point directionDeltas[4] = {
  {1, 0},   // RIGHT: Increase X by 1
  {-1, 0},  // LEFT: Decrease X by 1
  {0, 1},   // DOWN: Increase Y by 1
  {0, -1}   // UP: Decrease Y by 1
};

uint8_t lookupTable[3][3] = {
  { DOWN,    DOWN,    DOWN},  // Y-axis - joystick is pushed upward
  { RIGHT,   0x00,    LEFT},  // Center zone (no movement or small jitter around center)
  { UP,      UP,      UP }   // Y-axis - joystick is pushed downward
};

// Define barriers for number 9
barrier9 barriers[] = {
  {70, 140, 100, 70},  // top Barrier for number 9
  {150, 220, 20, 30},   // right barrier for number 9
};

//define global variables
int8_t snakeLength = 2;
int8_t segmentSize = 10;
int8_t headIndex = 1;
int8_t tailIndex = 0;
int8_t maxSnakeLength = 22;
int8_t currentDirection = RIGHT; // initial direction that snake travels when the game is start
const short threshold = 300; // threshold to detect the significant movement of the joystick

point snake[22]; // array to hold snake's all positions (circular buffer)

// variables for the random rectangle 
point rectPosition;

//screen dimensions 
short screenHeight = 320; //width of screen this represent as x 
short screenWidth = 240;

//time variables 
unsigned long previousTime = 0;
unsigned long delayTime = 250; 
unsigned long foodSpawnTime = 0;
unsigned long delayFoodTime = 5000;
bool foodActive = false;
int8_t timeLeft = 0;
unsigned long lastCountdownUpdate = 0;  // Track when the countdown was last updated
const int countdownInterval = 1000;     // Update countdown every 1000 ms (1 second)

//variables for red food
point redFoodPos[MAX_RED_FOOD]; 
bool redFoodActive[MAX_RED_FOOD] = {false};
unsigned int RedFoodSpawnTime[MAX_RED_FOOD];
unsigned int nextRedInterval[MAX_RED_FOOD];
int8_t activeRedFood = 1; // The number of red foods that are active initially

// score 
int8_t score = 0;
int8_t maxScore =0;

// levels 
int8_t currentLevel = 1;
const int8_t maxLevel = 5;
int8_t scoreThresholds[maxLevel] = {0, 2, 4, 6, 8};  // Array of score thresholds

// variable for barrier 
bool number9Drawn = false;
int8_t numBarriers = sizeof(barriers) / sizeof(barriers[0]);
bool game_Over = false;

bool isMainMenu = true;
bool newGame = false;
bool highScore = false;

void initSnake();
void mainMenu();
void handleMainMenu();
void drawSegment(short x, short y, uint16_t color);
void moveSnake();
void updateDIrection();
void generateRect();
bool checkEatFood(point point_1, point point_2);
void gameOver();
void checkSelfCollision ();
void updateScore();
void updateLevel();
void updateCountdown(int timeLeft);
void drawNumber9(int x, int y, int size, uint16_t color);
void changeLevel(int level);
void foodExpire();
void generateRedFood();
void handleRedFood();
void decreaseScore();
void level1Features();
void level2Features();
void level3Features();
void level4features();
void level5features();
void adjustGameDifficulty(int level);
void game_start_sound();
void game_over_sound();
void eat_Rfood_sound();
void eat_Wfood_sound();
void storeMaxScoreInEEPROM(int maxScore);
int readMaxScoreFromEEPROM();
void displayHighScore();

// Array of function pointers for level features
void (*levelFeatures[maxLevel])() = {level1Features, level2Features, level3Features, level4features, level5features};

void eat_Wfood_sound(){
  tone(BUZZER,200);
  delay(100);
  noTone(BUZZER);
  tone(BUZZER,240);
  delay(100);
  noTone(BUZZER);

}

void eat_Rfood_sound(){
  tone(BUZZER,600);
  delay(100);
  noTone(BUZZER);
}

void game_over_sound(){
  for (int i = 0; i < 7; i++) {
    tone(BUZZER,melody[i]);
    delay(300);
    noTone(BUZZER);
    delay(50);
  }
}

void game_start_sound(){
  for (int i = 7; i < 11; i++) {
    tone(BUZZER,melody[i]);
    delay(300);
    noTone(BUZZER);
    delay(50);
  }
}

void storeMaxScoreInEEPROM(int maxScore) {
  // Store the maximum score (2 bytes for an int) at address 0
  EEPROM.put(0, maxScore);
  
}

int readMaxScoreFromEEPROM() {
  int maxScore = 0;

  // Read the maximum score from EEPROM at address 0
  EEPROM.get(0, maxScore);

  if (maxScore == -1) {
    maxScore = 0;
    storeMaxScoreInEEPROM(maxScore);  // Initialize EEPROM with 0
  }

  return maxScore;
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);

  pinMode(VERT_PIN, INPUT); // pin configuration for joystick
  pinMode(HORZ_PIN, INPUT);
  pinMode(SELECT, INPUT_PULLUP);

  tft.begin();  // Set screen orientation
  tft.fillScreen(BACKGROUND_COLOR);  // Fill screen with black 
  //tft.fillRect(0, 0, 240, 70, 0x00FF);

  initSnake();
  maxScore = readMaxScoreFromEEPROM();

  //  for (int8_t i = 0; i < snakeLength; i++) {
  //    drawSegment(snake[i].x, snake[i].y, SNAKE_COLOR);
  //  }

  randomSeed(analogRead(A2)); //generate randome seed for the random number generator.

  // generateRect();
  // updateScore();
  // updateLevel();
}

void loop() { 
  if (!game_Over){
    if((!isMainMenu) && newGame){
      for (int8_t i = 0; i < maxLevel; i++) {
        if (score == scoreThresholds[i] && (currentLevel != i + 1) && (currentLevel <= 4)) {
          changeLevel(i + 1);  // Move to the corresponding level
          break;  // Exit the loop once the level is changed
        }
      }

      if (currentLevel > 5){
        levelFeatures[4]();
      }else{
        // Call the corresponding feature function for the new level
        levelFeatures[currentLevel - 1]();
      }
    }else{
      if(!highScore){
        mainMenu();
        handleMainMenu();
      }else{
        displayHighScore();
        if (digitalRead(SELECT) == LOW){
          tft.fillScreen(BACKGROUND_COLOR);
          highScore = false;
        }
      }
    }
  }else{
    gameOver();
  }
}

int getIndex(int value){  // return the direction according to joystick value in 0, 1 and 2
  if(value < 512 - threshold) return 0; // RIGHT/DOWN
  if(value > 512 + threshold) return 2; // LEFT/UP
  return 1;
}

void mainMenu(){
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextSize(3);
  tft.print("MAIN MENU");

  tft.setCursor(20, 140);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("LEFT:");

  tft.setCursor(80, 140);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextSize(2);
  tft.print("NEW GAME");

  tft.setCursor(20, 180);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("RIGHT:");

  tft.setCursor(100, 180);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextSize(2);
  tft.print("HIGH SCORE");
}

void displayHighScore(){
  tft.setCursor(20, 140);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("YOUR HIGH SCORE:");
  tft.print(readMaxScoreFromEEPROM());
}

void handleMainMenu(){
  short joystickInputHor = analogRead(HORZ_PIN);

  if (getIndex(joystickInputHor) == 2){
    newGame = true;
    isMainMenu = false;
    tft.fillScreen(BACKGROUND_COLOR);
    tft.fillRect(0, 0, 240, 70, 0x00FF);
    for (int8_t i = 0; i < snakeLength; i++) {
      drawSegment(snake[i].x, snake[i].y, SNAKE_COLOR);
    }
    generateRect();
    updateScore();
    updateLevel();
    game_start_sound();
    
  }else if((getIndex(joystickInputHor) == 0)){
    highScore = true;
    tft.fillScreen(BACKGROUND_COLOR);
  }
}

void initSnake() { //draw the snake with tail as index 0 and head as index 4
  for (int8_t i = 0; i < (snakeLength); i++){
    snake[i].x = (i + 1) * segmentSize;  
    snake[i].y = 120; // initial x position
  }
}

void drawSegment(short x, short y, uint16_t color) {// Draw a rectangle (segment)
  tft.fillRect(x, y, segmentSize, segmentSize, color); 
}

void moveSnake(){ // move the snake according to joystick input 
  unsigned long currentTime = millis(); // define the current time as milliseconds since the start of the programm 

  if ((currentTime - previousTime) >= delayTime){ // this code will run every 1 s (delayTime = 1)

    point newHead = snake[headIndex]; // get the current head position 

    newHead.x += directionDeltas[currentDirection - 1].x * segmentSize;  // update newhead by choosing the direction from directionDelta array
    newHead.y += directionDeltas[currentDirection - 1].y * segmentSize;

    if (newHead.x < 0){
      newHead.x = screenWidth;
    }else if (newHead.x > screenWidth){
      newHead.x = 0;
    }

    if (newHead.y < 70){
      newHead.y = screenHeight;
    }else if (newHead.y > screenHeight){
      newHead.y = 70;
    }

    headIndex = (headIndex + 1) % maxSnakeLength; // update the index of head in circular buffer 
    snake[headIndex] = newHead; // assign new head position

    if (!(checkEatFood(snake[headIndex], rectPosition))){ // If only snake did not eat the food then erase the tail. otherwise the tail remains.
      drawSegment(snake[tailIndex].x, snake[tailIndex].y, BACKGROUND_COLOR); // clear the tail in screen
      tailIndex = (tailIndex + 1) % maxSnakeLength; // update the index of tail in circular buffer
    }else{ 
      drawSegment(rectPosition.x, rectPosition.y, SNAKE_COLOR);
      snakeLength++;
      score++;
      if(score > maxScore){
        maxScore= score;
        storeMaxScoreInEEPROM(maxScore);
      }
      updateScore();
      generateRect();
      eat_Wfood_sound();
      //Serial.println(snakeLength);
    }
  

    drawSegment(snake[headIndex].x, snake[headIndex].y, SNAKE_COLOR); // update the next head position in screen 
    //Serial.print(snake[headIndex].x);
    //Serial.println(snake[headIndex].y);

    previousTime = currentTime;

  }
}

void updateDIrection(){  // update the direction 
  short joyHor = analogRead(HORZ_PIN); // read the analog pin values 
  short joyVer = analogRead(VERT_PIN);

  int8_t horizontalIndex = getIndex(joyHor);
  int8_t verticalIndex = getIndex(joyVer);

  uint8_t newDirection = lookupTable[verticalIndex][horizontalIndex];

  if(newDirection != 0x00) {  // If the joystick is in a valid zone (not center)
    if ((currentDirection == RIGHT && newDirection != LEFT) ||
        (currentDirection == LEFT && newDirection != RIGHT) ||
        (currentDirection == UP && newDirection != DOWN) ||
        (currentDirection == DOWN && newDirection != UP)) {
      currentDirection = newDirection;  // Update direction
    }
  }
}

void generateRect() {
  if (score >= 1){
    while(true){
      // Generate random x position, ensuring it's outside the barrier's x range
      rectPosition.x = random(0, screenWidth / segmentSize) * segmentSize;
      rectPosition.y = random(7, screenHeight / segmentSize) * segmentSize;

      if ((rectPosition.x >= 60 && rectPosition.x <= (180)) 
            && (rectPosition.y >= 120 && rectPosition.y <= (260))) {
        continue;  // Skip to next iteration if x is within the barrier's range
      }
      break;  // Exit loop if valid position is found
    }
  }else{
    rectPosition.x = random(0, screenWidth / segmentSize) * segmentSize;
    rectPosition.y = random(7, screenHeight / segmentSize) * segmentSize;
  }
  drawSegment(rectPosition.x, rectPosition.y, NORMAL_FOOD_COLOR);  // Draw the rectangle

  foodSpawnTime = millis();
  foodActive = true;
}

bool checkEatFood(point point_1, point point_2){
  return (point_1.x == point_2.x) && (point_1.y == point_2.y);
}

void checkSelfCollision (){
  for (int8_t i = 1; i < snakeLength; i++){
    int8_t bodyIndex = ((headIndex + maxSnakeLength) - i) % maxSnakeLength;

    if(snake[headIndex].x == snake[bodyIndex].x && snake[headIndex].y == snake[bodyIndex].y){
      tft.fillScreen(ILI9341_BLACK);
      game_Over = true;
      game_over_sound();
    }
  }
}

void gameOver() { // Function to display "OVER" when a collision occurs
  tft.setCursor(35, 120);
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(3);
  tft.println("GAME OVER!");

  tft.setCursor(60, 160);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.println("Press RESET");

  tft.setCursor(40, 200);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextSize(2);
  tft.print("YOUR SCORE: ");
  tft.print(score);
}

void updateScore(){
  tft.fillRect(80, 8, 40, 20, 0x00FF);
  tft.setCursor(5, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("SCORE: ");
  tft.print(score);
}

void changeLevel(int level) {// Function to change the level
  currentLevel = level;
  updateLevel();
}

void drawNumber9(int x, int y, int size, uint16_t color) {
  // Top horizontal bar
  tft.fillRect(x, y, 5 * size, size, color);   // Top horizontal line
  
  // Top right vertical bar
  tft.fillRect(x + 4 * size, y, size, 3 * size, color);  // Right vertical line (upper)

  // Middle horizontal bar
  tft.fillRect(x, y + 3 * size, 5 * size, size, color);  // Middle horizontal line

  // Bottom right vertical bar (closing the loop for the 9)
  tft.fillRect(x + 4 * size, y + 3 * size, size, 3 * size, color);  // Right vertical line (lower part)

  // Bottom horizontal bar
  //tft.fillRect(x, y + 6 * size, 5 * size, size, color);  // Bottom horizontal line

  // Left vertical bar for the top part of 9
  tft.fillRect(x, y, size, 4 * size, color);  // Left vertical line (upper part)
}

void checkCollisionWithBarrier() {

  for (int8_t i = 0; i < numBarriers; i++){
    if (snake[headIndex].x >= (barriers[i].x) && snake[headIndex].x <= (barriers[i].x + barriers[i].width - 10)
         && snake[headIndex].y >= (barriers[i].y) && snake[headIndex].y <= (barriers[i].y + barriers[i].height)){
      tft.fillScreen(ILI9341_BLACK);
      game_Over = true;
      game_over_sound();
      return;
    }
  }
}

void updateLevel(){
  // Clear the screen or update UI for the new level
  tft.fillRect(80, 40, 40, 20, 0x00FF);
  tft.setCursor(5, 40);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("LEVEL: ");
  tft.print(currentLevel);
}

void foodExpire(){

  if (foodActive){ 
    timeLeft = (delayFoodTime - (millis() - foodSpawnTime)) / 1000;

    // Only update the countdown if more than 1 second has passed
    if (millis() - lastCountdownUpdate >= countdownInterval) {
      updateCountdown(timeLeft);
      lastCountdownUpdate = millis();  // Reset the last update time
    }

    if((millis() - foodSpawnTime) >= delayFoodTime){

      drawSegment(rectPosition.x, rectPosition.y, BACKGROUND_COLOR);

      foodActive = false;
      generateRect();
    }
  }
}

void updateCountdown(int timeLeft){
  tft.fillRect(200, 35, 20, 20, 0x00FF);
  tft.setCursor(130, 40);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("TIME: ");
  tft.print(timeLeft);
}

void generateRedFood(int8_t index){
  if(!(redFoodActive[index])){
    while(true){
      // Generate random x position, ensuring it's outside the barrier's x range
      redFoodPos[index].x = random(0, screenWidth / segmentSize) * segmentSize;
      redFoodPos[index].y = random(7, screenHeight / segmentSize) * segmentSize;

      if ((redFoodPos[index].x >= 60 && redFoodPos[index].x <= (180)) 
          && (redFoodPos[index].y >= 120 && redFoodPos[index].y <= (260))) {
        continue;  // Skip to next iteration if x is within the barrier's range
      }
      break;  // Exit loop if valid position is found
    }
  }
  drawSegment(redFoodPos[index].x, redFoodPos[index].y, RED_FOOD);  // Draw the rectangle
  redFoodActive[index] = true;
  RedFoodSpawnTime[index] = millis();
}

void handleRedFood(){
  unsigned int currentRedTime = millis();
  for (int8_t i = 0; i < activeRedFood; i++)
    if(redFoodActive[i]){
      if((currentRedTime - RedFoodSpawnTime[i]) >= nextRedInterval[i]){
        drawSegment(redFoodPos[i].x, redFoodPos[i].y, BACKGROUND_COLOR);
        redFoodActive[i] = false;

        nextRedInterval[i] = random(1000, 4000);
      }
    }
    else{
      if((currentRedTime - RedFoodSpawnTime[i]) >= nextRedInterval[i]){
        generateRedFood(i);
      }
    }
}

void decreaseScore(){
  for (int8_t i = 0; i < activeRedFood; i++){
    if (checkEatFood(snake[headIndex], redFoodPos[i])){
      drawSegment(redFoodPos[i].x, redFoodPos[i].y, SNAKE_COLOR);
      redFoodActive[i] = false;
      score--;
      Serial.println(score);
      eat_Rfood_sound();
      updateScore();
    }
  }
}

void level1Features() {
  moveSnake();
  updateDIrection();
  //Serial.println(currentDirection);
  checkSelfCollision();
}

void level2Features() {

  if (!number9Drawn){
    drawNumber9(70, 140, 20, 0xFFE0);
    number9Drawn = true;
  }
  checkCollisionWithBarrier();
  checkSelfCollision();

  moveSnake();
  updateDIrection();
  // //Serial.println(currentDirection);
}

void level3Features(){

  checkCollisionWithBarrier();
  checkSelfCollision();

  moveSnake();
  updateDIrection();
  // //Serial.println(currentDirection);

  foodExpire();
}

void level4features(){
  checkCollisionWithBarrier();
  checkSelfCollision();

  moveSnake();
  updateDIrection();
  // //Serial.println(currentDirection);

  foodExpire();
  handleRedFood();
  decreaseScore();
}

void level5features(){
  checkCollisionWithBarrier();
  checkSelfCollision();

  moveSnake();
  updateDIrection();
  // //Serial.println(currentDirection);

  foodExpire();
  handleRedFood();
  decreaseScore();

  if (score >= 2*currentLevel){
    changeLevel(currentLevel+1);

    adjustGameDifficulty(currentLevel);
  }

  Serial.println(activeRedFood);
}

void adjustGameDifficulty(int level) {

  delayTime = delayTime - 50;
  if (level > 5 && activeRedFood < MAX_RED_FOOD) {
    activeRedFood = level - 4;  // Increase number of red foods after level 5
  }

}
