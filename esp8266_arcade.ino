#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ESP8266 Pins
const int btnLeft = 14;   // D5
const int btnSelect = 12; // D6
const int btnRight = 13;  // D7

// Master State Machine
int masterState = 0; // 0: Master Menu, 10+: Lights Out, 20+: Breakout, 30+: Block Jump, 40+: Simon Says, 50+: Minesweeper
int masterMenuOption = 0;

// ==========================================
// LIGHTS OUT VARIABLES & MATH ENGINE
// ==========================================
const bool inv3x3[9][9] = {
  {1, 0, 1, 0, 0, 1, 1, 1, 0}, {0, 0, 0, 0, 1, 0, 1, 1, 1}, {1, 0, 1, 1, 0, 0, 0, 1, 1},
  {0, 0, 1, 0, 1, 1, 0, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1, 0}, {1, 0, 0, 1, 1, 0, 1, 0, 0},
  {1, 1, 0, 0, 0, 1, 1, 0, 1}, {1, 1, 1, 0, 1, 0, 0, 0, 0}, {0, 1, 1, 1, 0, 0, 1, 0, 1}
};
bool grid[6][12]; 
int cursorIndex = 0; 
bool showHint = false;
int loMenuOption = 0;
int gridW = 3, gridH = 3, totalTiles = 9;
int spacing = 20, boxSize = 18, lightOffset = 4, lightSize = 10;

// ==========================================
// BREAKOUT VARIABLES & PHYSICS ENGINE
// ==========================================
float ballX, ballY, ballVX, ballVY;
int paddleX, paddleW = 24;
bool blocks[4][12]; // 4 rows, 12 columns
int blocksRemaining = 0;

// ==========================================
// BLOCK JUMP VARIABLES & PHYSICS ENGINE
// ==========================================
float gdPlayerY = 54, gdPlayerVY = 0;
const float gdGravity = 0.6;
const float gdJumpForce = -5.5;
bool gdIsGrounded = true;
int gdObsX = 128, gdObsW = 8, gdObsH = 12;
int gdScore = 0;

// ==========================================
// SIMON SAYS (MEMORY PATTERN) VARIABLES
// ==========================================
int simonSeq[32];
int simonStep = 0;
int simonRound = 0;
int simonCursor = 0;

// ==========================================
// MINESWEEPER VARIABLES
// ==========================================
byte msGrid[4][8]; // Bit 0: Mine, Bit 1: Revealed, Bit 2: Flagged, Bits 4-7: NeighborMineCount
int msCursor = 0;
bool msGameOver = false;
int msRevealedCount = 0;

// ==========================================
// SPACE SHOOTER VARIABLES
// ==========================================
int shipX = 60;
int laserX = -1, laserY = -1;
bool laserActive = false;
int alienX = 0, alienY = 0, alienDir = 2;
int shooterScore = 0;

// ==========================================
// TIC-TAC-TOE VARIABLES
// ==========================================
byte tttGrid[3][3]; // 0: Empty, 1: X, 2: O
int tttCursor = 0;
int tttTurn = 1;
int tttWinner = 0; // 0: None, 1: X, 2: O, 3: Tie
int tttMode = 0; // 0: 1 Player (vs CPU), 1: 2 Players
int tttMenuOption = 0;

// --- Lights Out Functions ---
void toggleTile(int x, int y) {
  if (x >= 0 && x < gridW && y >= 0 && y < gridH) { grid[y][x] = !grid[y][x]; }
}
void pressButton(int x, int y) {
  toggleTile(x, y); toggleTile(x - 1, y); toggleTile(x + 1, y);
  toggleTile(x, y - 1); toggleTile(x, y + 1);
}
void generatePuzzle() {
  for (int y = 0; y < gridH; y++) { for (int x = 0; x < gridW; x++) { grid[y][x] = false; } }
  int randomMoves = random(totalTiles * 2, totalTiles * 5);
  for (int i = 0; i < randomMoves; i++) { pressButton(random(gridW), random(gridH)); }
  cursorIndex = 0; masterState = 11;
  showHint = false;
}

// --- Breakout Functions ---
void initBreakout() {
  ballX = 64; ballY = 40;
  ballVX = 1.8; ballVY = -1.8;
  paddleX = 52;
  blocksRemaining = 0;
  
  for(int y = 0; y < 4; y++) {
    for(int x = 0; x < 12; x++) {
      if (random(100) > 30) { 
        blocks[y][x] = true;
        blocksRemaining++;
      } else {
        blocks[y][x] = false;
      }
    }
  }
  masterState = 20;
}

// --- Block Jump Functions ---
void initBlockJump() {
  gdPlayerY = 54;
  gdPlayerVY = 0;
  gdIsGrounded = true;
  gdObsX = 128;
  gdObsW = 8;
  gdObsH = random(8, 16); 
  gdScore = 0;
  masterState = 30;
}

// --- Simon Says Functions ---
void drawSimonGrid(int highlight) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(35, 2);
  display.print("SIMON SAYS");
  for (int i = 0; i < 4; i++) {
    int drawX = 12 + i * 26;
    int drawY = 24;
    display.drawRect(drawX, drawY, 22, 22, WHITE);
    if (i == highlight) {
      display.fillRect(drawX + 3, drawY + 3, 16, 16, WHITE);
    }
    if (masterState == 41 && simonCursor == i) {
      display.drawRect(drawX - 2, drawY - 2, 26, 26, WHITE);
    }
  }
  display.setCursor(10, 52);
  display.print("Round: "); display.print(simonRound);
  display.display();
}

void initSimon() {
  simonRound = 1;
  simonStep = 0;
  simonCursor = 0;
  for (int i = 0; i < 32; i++) {
    simonSeq[i] = random(4);
  }
  masterState = 40; 
}

// --- Minesweeper Functions ---
void revealTile(int x, int y) {
  if (x < 0 || x >= 8 || y < 0 || y >= 4) return;
  if (msGrid[y][x] & 0x02) return; // Already revealed
  if (msGrid[y][x] & 0x04) return; // Flagged
  
  msGrid[y][x] |= 0x02; // Set Revealed Bit
  msRevealedCount++;
  if (msGrid[y][x] & 0x01) {
    msGameOver = true;
    masterState = 51; 
    return;
  }
  
  int count = msGrid[y][x] >> 4;
  if (count == 0) {
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) continue;
        revealTile(x + dx, y + dy);
      }
    }
  }
}

void initMinesweeper() {
  msGameOver = false;
  msRevealedCount = 0;
  msCursor = 0;
  
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 8; x++) {
      msGrid[y][x] = 0;
    }
  }
  
  int placed = 0;
  while (placed < 6) {
    int rx = random(8);
    int ry = random(4);
    if (!(msGrid[ry][rx] & 0x01)) {
      msGrid[ry][rx] |= 0x01; 
      placed++;
    }
  }
  
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 8; x++) {
      if (msGrid[y][x] & 0x01) continue;
      int count = 0;
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          int ny = y + dy;
          int nx = x + dx;
          if (nx >= 0 && nx < 8 && ny >= 0 && ny < 4) {
            if (msGrid[ny][nx] & 0x01) count++;
          }
        }
      }
      msGrid[y][x] |= (count << 4);
    }
  }
  masterState = 50;
}

// --- Space Shooter Functions ---
void initShooter() {
  shipX = 60;
  laserActive = false;
  alienX = random(10, 110);
  alienY = 0;
  alienDir = 2;
  shooterScore = 0;
  masterState = 60;
}

// --- Tic-Tac-Toe Functions ---
void checkTTTWin() {
  for (int i = 0; i < 3; i++) {
    if (tttGrid[i][0] != 0 && tttGrid[i][0] == tttGrid[i][1] && tttGrid[i][1] == tttGrid[i][2]) tttWinner = tttGrid[i][0];
    if (tttGrid[0][i] != 0 && tttGrid[0][i] == tttGrid[1][i] && tttGrid[1][i] == tttGrid[2][i]) tttWinner = tttGrid[0][i];
  }
  if (tttGrid[0][0] != 0 && tttGrid[0][0] == tttGrid[1][1] && tttGrid[1][1] == tttGrid[2][2]) tttWinner = tttGrid[0][0];
  if (tttGrid[0][2] != 0 && tttGrid[0][2] == tttGrid[1][1] && tttGrid[1][1] == tttGrid[2][0]) tttWinner = tttGrid[0][2];
  
  if (tttWinner == 0) {
    bool full = true;
    for (int y = 0; y < 3; y++) {
      for (int x = 0; x < 3; x++) {
        if (tttGrid[y][x] == 0) full = false;
      }
    }
    if (full) tttWinner = 3;
  }
  
  if (tttWinner != 0) masterState = 71;
}

void initTicTacToe() {
  tttMenuOption = 0;
  masterState = 69; // Reroute to TTT Menu
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); 

  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnSelect, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  
  Wire.begin(5, 4);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  randomSeed(RANDOM_REG32); 
}

void loop() {
  // ==========================================
  // HOME SCREEN GLOBAL INTERRUPT (D5 Long Press)
  // ==========================================
  static unsigned long d5Timer = 0;
  if (digitalRead(btnLeft) == LOW) {
    if (d5Timer == 0) d5Timer = millis();
    else if (millis() - d5Timer > 1000) { 
      masterState = 0; 
      return; 
    }
  } else {
    d5Timer = 0;
  }

  // ==========================================
  // MASTER MENU STATE
  // ==========================================
  if (masterState == 0) {
    if (digitalRead(btnLeft) == LOW) { masterMenuOption = (masterMenuOption > 0) ? masterMenuOption - 1 : 6; delay(150); }
    if (digitalRead(btnRight) == LOW) { masterMenuOption = (masterMenuOption < 6) ? masterMenuOption + 1 : 0; delay(150); }
    if (digitalRead(btnSelect) == LOW) {
      delay(200);
      if (masterMenuOption == 0) masterState = 10; 
      if (masterMenuOption == 1) initBreakout();   
      if (masterMenuOption == 2) initBlockJump();
      if (masterMenuOption == 3) initSimon();      
      if (masterMenuOption == 4) initMinesweeper();
      if (masterMenuOption == 5) initShooter();
      if (masterMenuOption == 6) initTicTacToe();
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(20, 0); display.print("- ARCADE MENU -");
    // Dynamic Scrolling Menu logic to fit 7 items on a 64px screen
    const char* menuItems[] = {"Lights Out", "Breakout", "Block Jump", "Simon Says", "Minesweeper", "Space Shooter", "Tic-Tac-Toe"};
    int startItem = masterMenuOption >= 4 ? masterMenuOption - 3 : 0;
    
    for (int i = 0; i < 4; i++) {
      int idx = startItem + i;
      if (idx > 6) break;
      display.setCursor(10, 15 + (i * 12));
      if (masterMenuOption == idx) display.print("> ");
      display.print(menuItems[idx]);
    }
    
    display.display();
  }

  // ==========================================
  // LIGHTS OUT STATES
  // ==========================================
  else if (masterState == 10) { 
    if (digitalRead(btnLeft) == LOW) { loMenuOption = (loMenuOption > 0) ? loMenuOption - 1 : 2; delay(150); }
    if (digitalRead(btnRight) == LOW) { loMenuOption = (loMenuOption < 2) ? loMenuOption + 1 : 0; delay(150); }
    if (digitalRead(btnSelect) == LOW) {
      if (loMenuOption == 0) { gridW = 3; gridH = 3; spacing = 20; boxSize = 18; lightOffset = 4; lightSize = 10; }
      else if (loMenuOption == 1) { gridW = 5; gridH = 5; spacing = 12; boxSize = 10; lightOffset = 2; lightSize = 6; }
      else if (loMenuOption == 2) { gridW = 12; gridH = 6; spacing = 10; boxSize = 8; lightOffset = 2; lightSize = 4; }
      totalTiles = gridW * gridH;
      generatePuzzle();
      delay(200);
    }
    display.clearDisplay();
    display.setCursor(10, 10);
    display.print("Select Grid Size:");
    const char* options[] = {"3x3 (Easy)", "5x5 (Normal)", "12x6 (Extreme)"};
    for(int i = 0; i < 3; i++) {
      display.setCursor(20, 30 + (i * 10));
      if(i == loMenuOption) display.print("> ");
      display.print(options[i]);
    }
    display.display();
  } 
  
  else if (masterState == 11) { 
    if (digitalRead(btnLeft) == LOW) { cursorIndex = (cursorIndex > 0) ? cursorIndex - 1 : totalTiles - 1; delay(120); }
    if (digitalRead(btnRight) == LOW) { cursorIndex = (cursorIndex < totalTiles - 1) ? cursorIndex + 1 : 0; delay(120); }
    if (digitalRead(btnSelect) == LOW) {
      int holdTime = 0;
      while (digitalRead(btnSelect) == LOW && holdTime < 10) { delay(50); holdTime++; }
      if (holdTime >= 10 && gridW == 3) { showHint = !showHint; } 
      else {
        int cX = cursorIndex % gridW;
        int cY = cursorIndex / gridW;
        pressButton(cX, cY); showHint = false;
      }
      while (digitalRead(btnSelect) == LOW) { delay(10); }
      delay(50);
      
      bool allOff = true;
      for (int y = 0; y < gridH; y++) {
        for (int x = 0; x < gridW; x++) { if (grid[y][x] == true) allOff = false; }
      }
      if (allOff) masterState = 12;
    }

    display.clearDisplay();
    int offsetX = (128 - (gridW * spacing)) / 2;
    int offsetY = (64 - (gridH * spacing)) / 2;
    
    for (int y = 0; y < gridH; y++) {
      for (int x = 0; x < gridW; x++) {
        int drawX = offsetX + (x * spacing);
        int drawY = offsetY + (y * spacing);
        display.drawRect(drawX, drawY, boxSize, boxSize, WHITE);
        
        if (grid[y][x]) { display.fillRect(drawX + lightOffset, drawY + lightOffset, lightSize, lightSize, WHITE); }
        if (cursorIndex == (y * gridW + x)) { display.drawRect(drawX - 2, drawY - 2, boxSize + 4, boxSize + 4, WHITE); }
        
        if (showHint && gridW == 3) {
          bool needsPress = false;
          int tileIndex = y * 3 + x;
          for (int i = 0; i < 9; i++) {
            if (grid[i / 3][i % 3] && inv3x3[tileIndex][i]) needsPress = !needsPress;
          }
          if (needsPress) {
            display.drawLine(drawX, drawY, drawX + boxSize, drawY + boxSize, WHITE);
            display.drawLine(drawX + boxSize, drawY, drawX, drawY + boxSize, WHITE);
          }
        }
      }
    }
    display.display();
  } 
  
  else if (masterState == 12) { 
    display.clearDisplay();
    display.setCursor(25, 20); display.print("PUZZLE BEATEN!");
    display.setCursor(25, 40); display.print("D6: Main Menu");
    if (digitalRead(btnSelect) == LOW) { masterState = 0; delay(200); }
    display.display();
  }

  // ==========================================
  // BREAKOUT STATES
  // ==========================================
  else if (masterState == 20) { 
    if (digitalRead(btnLeft) == LOW) paddleX -= 3;
    if (digitalRead(btnRight) == LOW) paddleX += 3;
    
    if (paddleX < 0) paddleX = 0;
    if (paddleX > 128 - paddleW) paddleX = 128 - paddleW;

    ballX += ballVX;
    ballY += ballVY;
    if (ballX <= 0 || ballX >= 127) ballVX = -ballVX;
    if (ballY <= 0) ballVY = -ballVY;
    
    if (ballY >= 56 && ballY <= 60 && ballX >= paddleX && ballX <= paddleX + paddleW) {
      ballVY = -ballVY;
      ballY = 55; 
    }

    if (ballY < 24) { 
      int bX_idx = ballX / 11;
      int bY_idx = ballY / 6;  
      
      if (bX_idx >= 0 && bX_idx < 12 && bY_idx >= 0 && bY_idx < 4) {
        if (blocks[bY_idx][bX_idx]) {
          blocks[bY_idx][bX_idx] = false;
          ballVY = -ballVY;               
          blocksRemaining--;
        }
      }
    }

    if (ballY > 64 || blocksRemaining == 0) {
      masterState = 21;
      delay(500); 
    }

    display.clearDisplay();
    for(int y = 0; y < 4; y++) {
      for(int x = 0; x < 12; x++) {
        if (blocks[y][x]) { display.fillRect(x * 11, y * 6, 9, 4, WHITE); }
      }
    }
    
    display.fillRect(paddleX, 58, paddleW, 3, WHITE);
    display.fillRect(ballX, ballY, 2, 2, WHITE);
    display.display();
    delay(15); 
  }
  
  else if (masterState == 21) { 
    display.clearDisplay();
    display.setCursor(20, 20);
    if (blocksRemaining == 0) display.print("YOU WIN!");
    else display.print("GAME OVER");
    
    display.setCursor(20, 40); display.print("D6: Main Menu");
    if (digitalRead(btnSelect) == LOW) { masterState = 0; delay(200); }
    display.display();
  }

  // ==========================================
  // BLOCK JUMP STATES
  // ==========================================
  else if (masterState == 30) { 
    if (digitalRead(btnSelect) == LOW && gdIsGrounded) {
      gdPlayerVY = gdJumpForce;
      gdIsGrounded = false;
    }

    gdPlayerVY += gdGravity;
    gdPlayerY += gdPlayerVY;
    if (gdPlayerY >= 54) {
      gdPlayerY = 54;
      gdPlayerVY = 0;
      gdIsGrounded = true;
    }

    gdObsX -= 4; 
    
    if (gdObsX < -gdObsW) {
      gdObsX = 128;
      gdObsW = random(6, 12); 
      gdObsH = random(8, 20); 
      gdScore++;
    }

    int playerLeft = 20;
    int playerRight = 28;
    int playerTop = gdPlayerY;
    int playerBottom = gdPlayerY + 8;
    
    int obsLeft = gdObsX;
    int obsRight = gdObsX + gdObsW;
    int obsTop = 62 - gdObsH;
    int obsBottom = 62;
    
    if (playerRight > obsLeft && playerLeft < obsRight && playerBottom > obsTop && playerTop < obsBottom) {
      masterState = 31;
      delay(500); 
    }

    display.clearDisplay();
    display.drawLine(0, 62, 128, 62, WHITE); 
    display.fillRect(playerLeft, gdPlayerY, 8, 8, WHITE);
    display.fillRect(gdObsX, obsTop, gdObsW, gdObsH, WHITE); 
    
    display.setCursor(0, 0); 
    display.print("Score: "); display.print(gdScore);
    
    display.display();
    delay(15);
  }

  else if (masterState == 31) { 
    display.clearDisplay();
    display.setCursor(20, 10); display.print("GAME OVER");
    display.setCursor(20, 25);
    display.print("Score: "); display.print(gdScore);
    display.setCursor(20, 45); display.print("D6: Main Menu");
    
    if (digitalRead(btnSelect) == LOW) { masterState = 0; delay(200); }
    display.display();
  }

  // ==========================================
  // SIMON SAYS STATES
  // ==========================================
  else if (masterState == 40) {
    delay(800);
    for (int i = 0; i < simonRound; i++) {
      drawSimonGrid(simonSeq[i]);
      delay(400);
      drawSimonGrid(-1);
      delay(150);
    }
    masterState = 41; 
    simonStep = 0;
  }
  
  else if (masterState == 41) {
    if (digitalRead(btnLeft) == LOW) { simonCursor = (simonCursor > 0) ? simonCursor - 1 : 3; delay(150); }
    if (digitalRead(btnRight) == LOW) { simonCursor = (simonCursor < 3) ? simonCursor + 1 : 0; delay(150); }
    if (digitalRead(btnSelect) == LOW) {
      delay(200);
      if (simonCursor == simonSeq[simonStep]) {
        simonStep++;
        if (simonStep >= simonRound) {
          display.clearDisplay();
          display.setCursor(35, 25);
          display.print("CORRECT!");
          display.display();
          delay(1000);
          
          simonRound++;
          if (simonRound >= 32) {
            simonRound = 99; // Victory Flag
            masterState = 42;
          } else {
            simonSeq[simonRound - 1] = random(4);
            masterState = 40; 
          }
        }
      } else {
        masterState = 42; // Failure
      }
    }
    drawSimonGrid(-1);
  }
  
  else if (masterState == 42) {
    display.clearDisplay();
    display.setCursor(20, 15);
    if (simonRound == 99) {
      display.print("VICTORY WON!");
    } else {
      display.print("GAME OVER!");
      display.setCursor(20, 30);
      display.print("Reached Round: "); display.print(simonRound);
    }
    display.setCursor(20, 50);
    display.print("D6: Main Menu");
    if (digitalRead(btnSelect) == LOW) { masterState = 0; delay(200); }
    display.display();
  }

  // ==========================================
  // MINESWEEPER STATES
  // ==========================================
  else if (masterState == 50) {
    if (digitalRead(btnLeft) == LOW) { msCursor = (msCursor > 0) ? msCursor - 1 : 31; delay(120); }
    if (digitalRead(btnRight) == LOW) { msCursor = (msCursor < 31) ? msCursor + 1 : 0; delay(120); }
    
    if (digitalRead(btnSelect) == LOW) {
      int holdTime = 0;
      while (digitalRead(btnSelect) == LOW && holdTime < 10) { delay(50); holdTime++; }
      
      int cX = msCursor % 8;
      int cY = msCursor / 8;
      
      if (holdTime >= 10) {
        if (!(msGrid[cY][cX] & 0x02)) { msGrid[cY][cX] ^= 0x04; } // Toggle Flag
      } else {
        if (!(msGrid[cY][cX] & 0x04)) { 
          revealTile(cX, cY);
          if (!msGameOver) {
            bool won = true;
            for (int y = 0; y < 4; y++) {
              for (int x = 0; x < 8; x++) {
                if (!(msGrid[y][x] & 0x01) && !(msGrid[y][x] & 0x02)) { won = false; }
              }
            }
            if (won) { masterState = 51; }
          }
        }
      }
      while (digitalRead(btnSelect) == LOW) { delay(10); }
      delay(50);
    }
    
    display.clearDisplay();
    int offsetX = 16;
    int offsetY = 8;
    
    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < 8; x++) {
        int drawX = offsetX + x * 12;
        int drawY = offsetY + y * 12;
        
        display.drawRect(drawX, drawY, 10, 10, WHITE);
        
        if (msGrid[y][x] & 0x02) {
          if (msGrid[y][x] & 0x01) {
            display.fillCircle(drawX + 5, drawY + 5, 3, WHITE); // Mine
          } else {
            int count = msGrid[y][x] >> 4;
            if (count > 0) {
              display.setCursor(drawX + 3, drawY + 1);
              display.print(count);
            } else {
              display.drawPixel(drawX + 5, drawY + 5, WHITE);
            }
          }
        } else if (msGrid[y][x] & 0x04) {
          display.drawLine(drawX + 2, drawY + 2, drawX + 8, drawY + 8, WHITE);
          display.drawLine(drawX + 8, drawY + 2, drawX + 2, drawY + 8, WHITE); // Flag
        }
        
        if (msCursor == y * 8 + x) {
          display.drawRect(drawX - 2, drawY - 2, 14, 14, WHITE);
        }
      }
    }
    display.display();
  }
  
  else if (masterState == 51) {
    display.clearDisplay();
    display.setCursor(20, 15);
    if (msGameOver) {
      display.print("BOMB DETONATED!");
      display.setCursor(20, 30);
      display.print("GAME OVER");
    } else {
      display.print("MINEFIELD CLEAR!");
      display.setCursor(20, 30);
      display.print("YOU WIN!");
    }
    display.setCursor(20, 50);
    display.print("D6: Main Menu");
    if (digitalRead(btnSelect) == LOW) { masterState = 0; delay(200); }
    display.display();
  }

  // ==========================================
  // SPACE SHOOTER STATES
  // ==========================================
  else if (masterState == 60) {
    if (digitalRead(btnLeft) == LOW) shipX -= 4;
    if (digitalRead(btnRight) == LOW) shipX += 4;
    
    if (shipX < 0) shipX = 0;
    if (shipX > 116) shipX = 116;

    if (digitalRead(btnSelect) == LOW && !laserActive) {
      laserActive = true;
      laserX = shipX + 5;
      laserY = 54;
    }

    alienX += alienDir;
    if (alienX > 118 || alienX < 0) {
      alienDir = -alienDir;
      alienY += 6;
    }

    if (laserActive) {
      laserY -= 6;
      if (laserY < 0) laserActive = false;
      
      if (laserX >= alienX && laserX <= alienX + 10 && laserY <= alienY + 8 && laserY >= alienY) {
        laserActive = false;
        shooterScore++;
        alienY = 0;
        alienX = random(10, 110);
        alienDir = (alienDir > 0) ? alienDir + 1 : alienDir - 1; 
      }
    }

    if (alienY > 50) {
      masterState = 61;
      delay(500);
    }

    display.clearDisplay();
    display.fillRect(shipX, 58, 12, 6, WHITE); // Ship
    display.fillRect(shipX + 4, 54, 4, 4, WHITE); // Ship Turret
    display.fillRect(alienX, alienY, 10, 8, WHITE); // Alien
    
    if (laserActive) {
      display.drawLine(laserX, laserY, laserX, laserY + 4, WHITE);
    }
    
    display.setCursor(0, 0); 
    display.print(shooterScore);
    display.display();
    delay(15);
  }

  else if (masterState == 61) {
    display.clearDisplay();
    display.setCursor(20, 20); display.print("EARTH INVADED!");
    display.setCursor(20, 35); display.print("Score: ");
    display.print(shooterScore);
    display.setCursor(20, 50); display.print("D6: Main Menu");
    if (digitalRead(btnSelect) == LOW) { masterState = 0; delay(200); }
    display.display();
  }

  // ==========================================
  // TIC-TAC-TOE STATES
  // ==========================================
  else if (masterState == 69) { // TTT Mode Selection
    if (digitalRead(btnLeft) == LOW) { tttMenuOption = 0; delay(150); }
    if (digitalRead(btnRight) == LOW) { tttMenuOption = 1; delay(150); }
    
    if (digitalRead(btnSelect) == LOW) {
      tttMode = tttMenuOption;
      for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) tttGrid[y][x] = 0;
      }
      tttCursor = 0;
      tttTurn = 1;
      tttWinner = 0;
      masterState = 70;
      delay(200);
    }
    
    display.clearDisplay();
    display.setCursor(20, 10); display.print("TIC-TAC-TOE");
    display.setCursor(10, 30);
    if (tttMenuOption == 0) display.print("> ");
    display.print("1 Player (vs CPU)");
    display.setCursor(10, 45);
    if (tttMenuOption == 1) display.print("> ");
    display.print("2 Players");
    display.display();
  }

  else if (masterState == 70) {
    if (tttMode == 0 && tttTurn == 2) { // CPU TURN (Minimax Heuristic)
      delay(500); 
      int choice = -1;
      
      // 1. Evaluate immediate Win (2) or Block (1)
      for (int p = 2; p >= 1; p--) {
        if (choice != -1) break;
        for (int i = 0; i < 9; i++) {
          int r = i / 3, c = i % 3;
          if (tttGrid[r][c] == 0) {
            tttGrid[r][c] = p;
            bool wins = false;
            for (int j = 0; j < 3; j++) {
              if (tttGrid[j][0] == p && tttGrid[j][1] == p && tttGrid[j][2] == p) wins = true;
              if (tttGrid[0][j] == p && tttGrid[1][j] == p && tttGrid[2][j] == p) wins = true;
            }
            if (tttGrid[0][0] == p && tttGrid[1][1] == p && tttGrid[2][2] == p) wins = true;
            if (tttGrid[0][2] == p && tttGrid[1][1] == p && tttGrid[2][0] == p) wins = true;
            tttGrid[r][c] = 0; 
            if (wins) { choice = i; break; }
          }
        }
      }
      
      // 2. Hardware Entropy Fallback
      if (choice == -1) {
        int emptyCells[9], emptyCount = 0;
        for(int i = 0; i < 9; i++) {
          if(tttGrid[i / 3][i % 3] == 0) { emptyCells[emptyCount++] = i; }
        }
        if(emptyCount > 0) choice = emptyCells[random(emptyCount)];
      }
      
      if (choice != -1) {
        tttGrid[choice / 3][choice % 3] = 2;
        tttTurn = 1;
        checkTTTWin();
      }
      
    } else { // PLAYER TURN OR 2P MODE
      if (digitalRead(btnLeft) == LOW) { tttCursor = (tttCursor > 0) ? tttCursor - 1 : 8; delay(150); }
      if (digitalRead(btnRight) == LOW) { tttCursor = (tttCursor < 8) ? tttCursor + 1 : 0; delay(150); }
      
      if (digitalRead(btnSelect) == LOW) {
        delay(200);
        int cX = tttCursor % 3;
        int cY = tttCursor / 3;
        
        if (tttGrid[cY][cX] == 0) {
          tttGrid[cY][cX] = tttTurn;
          tttTurn = (tttTurn == 1) ? 2 : 1;
          checkTTTWin();
        }
      }
    }

    display.clearDisplay();
    int ox = 34;
    int oy = 2;
    display.drawLine(ox + 20, oy, ox + 20, oy + 60, WHITE);
    display.drawLine(ox + 40, oy, ox + 40, oy + 60, WHITE);
    display.drawLine(ox, oy + 20, ox + 60, oy + 20, WHITE);
    display.drawLine(ox, oy + 40, ox + 60, oy + 40, WHITE);
    
    for (int y = 0; y < 3; y++) {
      for (int x = 0; x < 3; x++) {
        int drawX = ox + x * 20 + 10;
        int drawY = oy + y * 20 + 10;
        
        if (tttGrid[y][x] == 1) {
          display.drawLine(drawX - 6, drawY - 6, drawX + 6, drawY + 6, WHITE);
          display.drawLine(drawX + 6, drawY - 6, drawX - 6, drawY + 6, WHITE);
        } else if (tttGrid[y][x] == 2) {
          display.drawCircle(drawX, drawY, 6, WHITE);
        }
        
        // Hide cursor during CPU turn
        if (tttCursor == y * 3 + x && !(tttMode == 0 && tttTurn == 2)) {
          display.drawRect(drawX - 8, drawY - 8, 17, 17, WHITE);
        }
      }
    }
    
    display.setCursor(0, 0);
    display.print(tttTurn == 1 ? "P1" : (tttMode == 0 ? "CPU" : "P2"));
    display.display();
  }
  
  else if (masterState == 71) {
    display.clearDisplay();
    display.setCursor(30, 20);
    
    if (tttWinner == 1) display.print("PLAYER 1 WINS!");
    else if (tttWinner == 2) {
      if (tttMode == 0) display.print("CPU WINS!"); 
      else display.print("PLAYER 2 WINS!"); 
    }
    else display.print("IT'S A TIE!");
    
    display.setCursor(30, 40); display.print("D6: Main Menu");
    if (digitalRead(btnSelect) == LOW) { masterState = 0; delay(200); }
    display.display();
  }
}
