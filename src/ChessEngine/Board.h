#pragma once

#include "utils.h"

struct StateHistory { // For things not shown by moves
    uint8_t castling_rights{0xF}; // 4-bit mask for KQkq
    Square en_passant_sq{SQ_NONE};
    uint8_t halfmove_clock;
    Piece captured_piece;
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

    std::vector<Move> generate_pseudo_legal_moves(bool capturesOnly=false);

    bool check_castle(U64 occupancy_path, U64 relevant_squares);

    void get_piece_moves(std::vector<Move> &moves, std::vector<Move> &captures, PieceType piece, bool capturesOnly);

    void get_pawn_moves(std::vector<Move> &moves, std::vector<Move> &captures, bool capturesOnly);

    void make_move(Move move);

    void unmake_move(Move move);

    // For testing and debugging

    void printAllBoards();

    bool operator==(const Board& other) const;

};
