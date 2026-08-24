#pragma once

#include <algorithm>
#include "utils.h"

struct StateHistory { // For things not shown by moves
    uint8_t castling_rights{0xF}; // 4-bit mask for KQkq
    Square en_passant_sq{SQ_NONE};
    uint8_t halfmove_clock;
    Piece captured_piece;
};

struct MoveList {
    std::array<Move, 256> moves;
    std::array<int, 256> scores;
    int count = 0;

    void push(Move move) {
        moves[count++] = move;
    }

    int size() const { return count; }
    Move& operator[](int index) { return moves[index]; }
    const Move& operator[](int index) const { return moves[index]; }
};

struct Board {
    // 12 Direct Bitboards (6 piece types * 2 colors)
    // pieces[WHITE][PAWN], pieces[BLACK][KNIGHT], etc.
    std::array<std::array<U64, 6>, 2> piece_type_bb{};
    std::array<U64, 3> occupancy{}; // WHITE, BLACK, BOTH

    // Mailbox array for O(1) piece lookup on a specific square
    std::array<Piece, 64> grid{};
    
    Color turn{};
    Color oppSide{};

    uint8_t castling_rights{0xF}; // 4-bit mask for KQkq
    Square en_passant_sq{SQ_NONE};

    uint8_t halfmove_clock;
    uint16_t fullmove_no;

    Piece captured_piece{NO_PIECE};

    std::array<StateHistory, 256> history;
    uint16_t ply_counter{0};

    Board(std::string_view fen);

    // Board(std::array<Piece, 64> g) : grid{g} {}

    bool is_square_attacked(Square sq);

    bool is_king_capturable();

    void generate_pseudo_legal_moves(MoveList &move_list, bool capturesOnly=false);

    void score_moves(MoveList &move_list);

    bool check_castle(U64 occupancy_path, U64 relevant_squares);

    void get_piece_moves(MoveList &move_list, PieceType piece, bool capturesOnly);

    void get_pawn_moves(MoveList &move_list, bool capturesOnly);

    void make_move(Move move);

    void unmake_move(Move move);

    // For testing and debugging

    void printAllBoards();

    bool operator==(const Board& other) const;

};
