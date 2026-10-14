#include <iostream>
#include <limits> // 用于处理输入错误

using namespace std;

// 全局变量：棋盘
char board[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
char currentPlayer = 'X';

// 1. 绘制棋盘
void drawBoard() {
    cout << "\n";
    cout << " " << board[0] << " | " << board[1] << " | " << board[2] << " \n";
    cout << "---+---+---\n";
    cout << " " << board[3] << " | " << board[4] << " | " << board[5] << " \n";
    cout << "---+---+---\n";
    cout << " " << board[6] << " | " << board[7] << " | " << board[8] << " \n";
    cout << "\n";
}

// 2. 检查是否获胜
bool checkWin(char player) {
    // 检查行
    for (int i = 0; i < 9; i += 3) {
        if (board[i] == player && board[i+1] == player && board[i+2] == player)
            return true;
    }
    // 检查列
    for (int i = 0; i < 3; ++i) {
        if (board[i] == player && board[i+3] == player && board[i+6] == player)
            return true;
    }
    // 检查对角线
    if (board[0] == player && board[4] == player && board[8] == player)
        return true;
    if (board[2] == player && board[4] == player && board[6] == player)
        return true;

    return false;
}

// 3. 检查是否平局
bool checkDraw() {
    for (int i = 0; i < 9; ++i) {
        // 如果还有数字，说明没下满
        if (board[i] != 'X' && board[i] != 'O')
            return false;
    }
    return true;
}

// 4. 切换玩家
void switchPlayer() {
    currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';
}

// 5. 游戏主循环
void playGame() {
    bool gameOver = false;

    while (!gameOver) {
        drawBoard();
        int move;
        
        // 获取玩家输入，并处理非法输入（防止死循环）
        cout << "玩家 " << currentPlayer << "，请输入位置 (1-9): ";
        while (!(cin >> move) || move < 1 || move > 9 || board[move - 1] == 'X' || board[move - 1] == 'O') {
            // 清除错误状态并清空输入缓冲区
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "无效输入！请输入 1-9 之间的数字，且该位置未被占用: ";
        }

        // 更新棋盘
        board[move - 1] = currentPlayer;

        // 判断游戏状态
        if (checkWin(currentPlayer)) {
            drawBoard();
            cout << "🎉 恭喜！玩家 " << currentPlayer << " 获胜！\n";
            gameOver = true;
        } 
        else if (checkDraw()) {
            drawBoard();
            cout << "🤝 平局！游戏结束。\n";
            gameOver = true;
        } 
        else {
            switchPlayer();
        }
    }
}

int main() {
    cout << "=== 欢迎来到井字棋游戏 ===\n";
    cout << "玩家1使用 'X'，玩家2使用 'O'。\n";
    cout << "请输入数字 1-9 来落子，对应棋盘上的位置。\n";
    
    playGame();

    cout << "感谢游玩！\n";
    return 0;
}
