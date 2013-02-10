#include "dropblox_ai.h"
#include <algorithm>
#include <cassert>

using namespace json;
using namespace std;

//----------------------------------
// Block implementation starts here!
//----------------------------------

Block::Block(Object& raw_block) {
    center.i = (int)(Number&)raw_block["center"]["i"];
    center.j = (int)(Number&)raw_block["center"]["j"];
    size = 0;

    Array& raw_offsets = raw_block["offsets"];
    for (Array::const_iterator it = raw_offsets.Begin(); it < raw_offsets.End(); it++) {
        size += 1;
    }
    for (int i = 0; i < size; i++) {
        offsets[i].i = (Number&)raw_offsets[i]["i"];
        offsets[i].j = (Number&)raw_offsets[i]["j"];
    }

    translation.i = 0;
    translation.j = 0;
    rotation = 0;
}

void Block::left() {
    translation.j -= 1;
}

void Block::right() {
    translation.j += 1;
}

void Block::up() {
    translation.i -= 1;
}

void Block::down() {
    translation.i += 1;
}

void Block::rotate() {
    rotation += 1;
}

void Block::unrotate() {
    rotation -= 1;
}

// The checked_* methods below perform an operation on the block
// only if it's a legal move on the passed in board.  They
// return true if the move succeeded.
//
// The block is still assumed to start in a legal position.
bool Block::checked_left(const Board& board) {
    left();
    if (board.check(*this)) {
        return true;
    }
    right();
    return false;
}

bool Block::checked_right(const Board& board) {
    right();
    if (board.check(*this)) {
        return true;
    }
    left();
    return false;
}

bool Block::checked_up(const Board& board) {
    up();
    if (board.check(*this)) {
        return true;
    }
    down();
    return false;
}

bool Block::checked_down(const Board& board) {
    down();
    if (board.check(*this)) {
        return true;
    }
    up();
    return false;
}

bool Block::checked_rotate(const Board& board) {
    rotate();
    if (board.check(*this)) {
        return true;
    }
    unrotate();
    return false;
}

void Block::do_command(const string& command) {
    if (command == "left") {
        left();
    } else if (command == "right") {
        right();
    } else if (command == "up") {
        up();
    } else if (command == "down") {
        down();
    } else if (command == "rotate") {
        rotate();
    } else {
        throw Exception("Invalid command " + command);
    }
}

void Block::do_commands(const vector<string>& commands) {
    for (int i = 0; i < (int) commands.size(); i++) {
        do_command(commands[i]);
    }
}

void Block::reset_position() {
    translation.i = 0;
    translation.j = 0;
    rotation = 0;
}

//----------------------------------
// Board implementation starts here!
//----------------------------------

Board::Board() {
    rows = ROWS;
    cols = COLS;
}

Board::Board(Object& state) {
    rows = ROWS;
    cols = COLS;

    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            bitmap[i][j] = ((int)(Number&)state["bitmap"][i][j] ? 1 : 0);
        }
    }

    // Note that these blocks are NEVER destructed! This is because calling
    // place() on a board will create new boards which share these objects.
    //
    // There's a memory leak here, but it's okay: blocks are only constructed
    // when you construct a board from a JSON Object, which should only happen
    // for the very first board. The total memory leaked will only be ~10 kb.
    block = new Block(state["block"]);
    for (int i = 0; i < PREVIEW_SIZE; i++) {
        preview.push_back(new Block(state["preview"][i]));
    }
}

// Returns true if the `query` block is in valid position - that is, if all of
// its squares are in bounds and are currently unoccupied.
bool Board::check(const Block& query) const {
    Point point;
    for (int i = 0; i < query.size; i++) {
        point.i = query.center.i + query.translation.i;
        point.j = query.center.j + query.translation.j;
        if (query.rotation % 2) {
            point.i += (2 - query.rotation)*query.offsets[i].j;
            point.j +=  -(2 - query.rotation)*query.offsets[i].i;
        } else {
            point.i += (1 - query.rotation)*query.offsets[i].i;
            point.j += (1 - query.rotation)*query.offsets[i].j;
        }
        if (point.i < 0 || point.i >= ROWS ||
            point.j < 0 || point.j >= COLS || bitmap[point.i][point.j]) {
            return false;
        }
    }
    return true;
}

// Resets the block's position, moves it according to the given commands, then
// drops it onto the board. Returns a pointer to the new board state object.
//
// Throws an exception if the block is ever in an invalid position.
Board* Board::do_commands(const vector<string>& commands) {
    block->reset_position();
    if (!check(*block)) {
        throw Exception("Block started in an invalid position");
    }
    for (int i = 0; i < (int) commands.size(); i++) {
        if (commands[i] == "drop") {
            return place();
        } else {
            block->do_command(commands[i]);
            if (!check(*block)) {
                throw Exception("Block reached in an invalid position");
            }
        }
    }
    // If we've gotten here, there was no "drop" command. Drop anyway.
    return place();
}

// Drops the block from whatever position it is currently at. Returns a
// pointer to the new board state object, with the next block drawn from the
// preview list.
//
// Assumes the block starts out in valid position.
// This method translates the current block downwards.
//
// If there are no blocks left in the preview list, this method will fail badly!
// This is okay because we don't expect to look ahead that far.

Board* Board::place(int *score) {
    Board* new_board = new Board();
    Block new_block = *block;
    new_board->block = &new_block;

    while (check(new_block)) {
        new_block.down();
    }
    new_block.up();

    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            new_board->bitmap[i][j] = bitmap[i][j];
        }
    }

    Point point;
    for (int i = 0; i < new_block.size; i++) {
        point.i = new_block.center.i + new_block.translation.i;
        point.j = new_block.center.j + new_block.translation.j;
        if (new_block.rotation % 2) {
            point.i += (2 - new_block.rotation)*new_block.offsets[i].j;
            point.j +=  -(2 - new_block.rotation)*new_block.offsets[i].i;
        } else {
            point.i += (1 - new_block.rotation)*new_block.offsets[i].i;
            point.j += (1 - new_block.rotation)*new_block.offsets[i].j;
        }
        new_board->bitmap[point.i][point.j] = 1;
    }
    int rows_removed = Board::remove_rows(&(new_board->bitmap));

    if (score != NULL)
        *score = (1 << rows_removed) - 1;

    new_board->block = preview[0];
    for (int i = 1; i < (int) preview.size(); i++) {
        new_board->preview.push_back(preview[i]);
    }

    return new_board;
}

Board* Board::place() {
    return place(NULL);
}

// A static method that takes in a new_bitmap and removes any full rows from it.
// Mutates the new_bitmap in place.
int Board::remove_rows(Bitmap* new_bitmap) {
    int rows_removed = 0;
    for (int i = ROWS - 1; i >= 0; i--) {
        bool full = true;
        for (int j = 0; j < COLS; j++) {
            if (!(*new_bitmap)[i][j]) {
                full = false;
                break;
            }
        }
        if (full) {
            rows_removed += 1;
        } else if (rows_removed) {
            for (int j = 0; j < COLS; j++) {
                (*new_bitmap)[i + rows_removed][j] = (*new_bitmap)[i][j];
            }
        }
    }
    for (int i = 0; i < rows_removed; i++) {
        for (int j = 0; j < COLS; j++) {
            (*new_bitmap)[i][j] = 0;
        }
    }
    return rows_removed;
}

// END OF PREWRITTEN CODE

int holes[ROWS][COLS];
int heuristic(Bitmap* bitmap, int score) {
    int h = 0;

    /* score multipliers */
    int SCORE_MULT = 100;
    int HOLE_MULT = 3;
    int EVEN_MULT = 1;

    h += score * SCORE_MULT;

    /* mark holes */
  
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            int above = 0;
            if ((*bitmap)[i][j]) continue;
            for (int bi = i - 1; bi >= 0; bi--) {
                if ((*bitmap)[bi][j]) {
                    above = 1; break;
                }
            }
            if (i == 0)
                above = 1;
            holes[i][j] = above;

        }
    }

    /* count number of rows with holes */
    int h_row_holes = 0;
    for (int i = 0; i < ROWS; i++) {
        int holes_in_row = 0;
        for (int j = 0; j < COLS; j++) {
            holes_in_row += holes[i][j];
            
        }

        if (holes_in_row > 0)
            h_row_holes += holes_in_row;
    }

    h -= h_row_holes * HOLE_MULT;


    /* check evenness of board */
    int evenness = 0;
    int prev_height = 0;
    int r = 0, j = 0, diff;
    while (r < ROWS && !(*bitmap)[r][j]) r++;
    prev_height = r;

    for(j = 1; j < COLS; j++) {
        r = 0;
        while (r < ROWS && !(*bitmap)[r][j]) r++;
        diff = prev_height - r;
        if (diff < 0) diff *= -1;
        evenness += diff * diff;
        prev_height = r;
    }
    h -= EVEN_MULT * evenness;

    return h;
}

const int AHEAD = 5, ROTATIONS = 4, NUM_KEEP = 50;

struct extra {
    // Number of times to move right from the left edge; number of rotations; score added.
    int rights;
    int rotates;
    int score;

    extra(int a = 0, int b = 0, int c = 0) {
        rights = a;
        rotates = b;
        score = c;
    }
};

int main(int argc, char** argv) {
    // Construct a JSON Object with the given game state.
    assert(argc >= 2);
    istringstream raw_state(argv[1]);
    Object state;
    Reader::Read(state, raw_state);

    // Construct a board from this Object.
    Board *initial = new Board(state);
    vector<pair<Board*, extra> > candidates;
    candidates.push_back(make_pair(initial, extra(-1, -1, 0)));

    for (int x = 0; x < AHEAD; x++) {
        vector<pair<Board*, extra> > new_candidates;
        //fprintf(stderr, "%d candidates\n", (int) candidates.size());

        for (int c = 0; c < (int) candidates.size(); c++) {
            Board *board = candidates[c].first;
            extra e = candidates[c].second;

            // Move it all the way up and left
            while (board->block->checked_up(*board))
                ;

            while (board->block->checked_left(*board))
                ;

            int rights = 0;

            do {
                for (int i = 0; i < ROTATIONS; i++) {
                    int score = 0;
                    Block copy_block = *board->block;
                    Board *new_board = board->place(&score);
                    extra new_e = e;
                    new_e.score += score;

                    // Set first move but only if it's first
                    if (new_e.rights == -1) {
                        new_e.rights = rights;
                        new_e.rotates = i;
                    }

                    new_candidates.push_back(make_pair(new_board, new_e));
                    board->block = &copy_block;
                    board->block->checked_rotate(*board);
                }

                rights++;
            } while (board->block->checked_right(*board));
        }

        // Check by hashing that everything is distinct

        // Remove all but the best NUM_KEEP
        vector<pair<int, int> > sorted_candidates;

        for (int i = 0; i < (int) new_candidates.size(); i++) {
            Board *board = new_candidates[i].first;
            // TODO: see if these parentheses are really necessary
            int value = heuristic(&(board->bitmap), new_candidates[i].second.score);
            sorted_candidates.push_back(make_pair(value, i));
        }

        sort(sorted_candidates.rbegin(), sorted_candidates.rend());
        candidates.clear();

        for (int i = 0; i < min((int) sorted_candidates.size(), NUM_KEEP); i++)
            candidates.push_back(new_candidates[sorted_candidates[i].second]);

        //fprintf(stderr, "X is %d\n", x);
    }

    Board *board = new Board(state);
    extra move = candidates[0].second;
    fprintf(stderr, "%d %d %d\n", move.rights, move.rotates, move.score);

    // Make some moves!
    vector<string> moves;

    while (board->block->checked_left(*board)) {
        moves.push_back("left");
    }

    for (int i = 0; i < move.rights; i++)
        moves.push_back("right");

    for (int i = 0; i < move.rotates; i++)
        moves.push_back("rotate");

    for (int i = 0; i < (int) moves.size(); i++) {
        cout << moves[i] << '\n';
    }

    cout << flush;
    return 0;
}
