#include <emscripten.h>
#include <chrono>
#include "Board.h"
#include "eval.h"

// TODOs:
// - Implement move ordering
// - Implement transposition table
// - Implement iterative deepening
// - Look into using NNUE as evaluation instead
// - Look into multithreading with web workers

constexpr int INF = 1e9;

uint64_t total_nodes = 0;

int quiescence(uint8_t ply,int alpha, int beta, Board& board) {
    total_nodes++;
    
    Square king_pos = static_cast<Square>(std::countr_zero(board.piece_type_bb[board.turn][KING]));
    bool in_check = board.is_square_attacked(king_pos);
    
    if (!in_check) {
        int stand_pat = pesto_eval(board);
        if (stand_pat >= beta) return beta;
        if (alpha < stand_pat) alpha = stand_pat;
    }

    std::vector<Move> moves = in_check ? board.generate_pseudo_legal_moves() : board.generate_pseudo_legal_moves(true); // Generate all moves if in check, otherwise only captures

    if (in_check && moves.size() == 0) {
        return -INF + ply; // Mate detected inside QS
    }
    
    for (const Move& move : moves) {
        board.make_move(move);
        if (!board.is_king_capturable()) {
            int score = -quiescence(ply + 1, -beta, -alpha, board);
            board.unmake_move(move);

            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
        } else {
            board.unmake_move(move);
        }
    }
    return alpha;
}

int negamax(uint8_t depth, uint8_t ply, Board& board, int alpha, int beta) {
    total_nodes++;

    if (depth == 0) {
        return quiescence(ply, alpha, beta, board);
    }

    int max = -INF;
    std::vector<Move> moves = board.generate_pseudo_legal_moves();
    uint8_t legal_moves_count = 0;
    for (Move move : moves) {
        board.make_move(move);
        if (!board.is_king_capturable()) {
            legal_moves_count++;
            int score = -negamax(depth - 1, ply + 1, board, -beta, -alpha);
            max = std::max(max, score);
            alpha = std::max(max, alpha);
            if (alpha >= beta) {
                // std::println("cutoff reached"); 
                board.unmake_move(move);
                break;
            }
        }
        board.unmake_move(move);
    }

    if (legal_moves_count == 0) {
        Square king_pos = static_cast<Square>(std::countr_zero(board.piece_type_bb[board.turn][KING]));
        if (board.is_square_attacked(king_pos)) { // in check
            return -INF + ply; // Checkmate, return negative score with depth to prefer faster checkmates
        }
        return 0; // stalemate
    }

    return max;
}

Move choose_negamax_move(uint8_t depth, Board& board) {
    // uint64_t nodes = 0;
    total_nodes++;

    int max = -INF;
    std::vector<Move> moves = board.generate_pseudo_legal_moves();
    Move bestMove;
    uint8_t ply = 0;
    for (Move move : moves) {
    // Move move = moves[0];
        board.make_move(move);
        if (!board.is_king_capturable()) {
            // printMove(move);
            int score = -negamax(depth - 1, ply + 1, board, -INF, INF);
            // std::println("Score: {}", score);
            if (score > max) {
                max = score;
                bestMove = move;
            }
        }
        board.unmake_move(move);
    }

    // board.printAllBoards();
    // std::println("Current pesto_eval for {}: {}", board.turn == WHITE ? "white" : "black", pesto_eval(board));
    std::println("Best score: {}", max);
    std::println("Best Move: {}", moveToString(bestMove));

    return bestMove;
}

static bool engine_initialized = init_tables();

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    const char* get_best_move(const char* fen_str, int depth) {
        std::string_view fen{fen_str};
        // std::string_view initialfen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        // std::string_view testfen = "7k/8/8/7p/7q/8/4K3/1q1r4 w - - 12 68";

        std::println("FEN: {}", fen);

        Board board{fen};
        // Board board{testfen};
        // Board board{initialfen};
        // Board board2{testfen};

        auto start = std::chrono::high_resolution_clock::now();
        total_nodes = 0;

        static std::string move_buffer;
        move_buffer = moveToString(choose_negamax_move(depth, board));
        // move_buffer = "e2e4";

        auto end = std::chrono::high_resolution_clock::now();
        double seconds = std::chrono::duration<double>(end - start).count();
        double nps = total_nodes / seconds;

        std::cout << "Depth: " << depth 
                << " | Nodes: " << total_nodes 
                << " | Time: " << seconds << "s" 
                << " | NPS: " << static_cast<uint64_t>(nps) << std::endl;

        // std::println("QS Nodes searched: {}", qs_nodes);

        std::cout << std::endl;

        return move_buffer.c_str();
    }
}

uint64_t perft(int depth, Board& board) { // For testing correctness of move generation and
    if (depth == 0) return 1ULL;

    uint64_t nodes = 0;
    std::vector<Move> move_list = board.generate_pseudo_legal_moves();

    for (int i = 0; i < move_list.size(); ++i) {
        Move move = move_list[i];

        board.make_move(move);
        if (board.is_king_capturable()) {
            board.unmake_move(move); 
            continue;
        }

        nodes += perft(depth - 1, board);

        board.unmake_move(move);
    }

    return nodes;
}

void perft_divide(int depth, Board& board, uint64_t& total_nodes) {
    std::vector<Move> move_list = board.generate_pseudo_legal_moves();

    for (int i = 0; i < move_list.size(); ++i) {
        Move move = move_list[i];

        board.make_move(move);
        if (board.is_king_capturable()) {
            board.unmake_move(move);
            continue;
        }

        uint64_t nodes = perft(depth - 1, board);
        total_nodes += nodes;
        board.unmake_move(move);

        uint8_t to = (move >> 6) & 0x3F;
        uint8_t from = move & 0x3F;
        std::println("{}{}: {}", getStringFromSquare(from), getStringFromSquare(to), nodes);
    }

    std::cout << "\nTotal Nodes at Depth " << depth << ": " << total_nodes << "\n";
}
