#include <emscripten.h>
#include <chrono>
#include "Board.h"
#include "eval.h"

// TODOs:
// - Put site link on resume and update and apply to jobs
// - Implement transposition table
// - Implement iterative deepening
// - Maybe implement killer move heuristic
// - Maybe implement history heuristic
// - Look into using NNUE as evaluation instead
// - Look into multithreading with web workers

constexpr int INF = 1e9;

uint64_t total_nodes = 0;

int quiescence(int ply,int alpha, int beta, Board& board) {
    total_nodes++;
    
    Square king_pos = static_cast<Square>(std::countr_zero(board.piece_type_bb[board.turn][KING]));
    bool in_check = board.is_square_attacked(king_pos);
    
    if (!in_check) {
        int stand_pat = pesto_eval(board);
        if (stand_pat >= beta) return beta;
        if (alpha < stand_pat) alpha = stand_pat;
    }

    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list, true); // Only captures
    board.score_moves(move_list);
    int legal_moves_count = 0;
    
    for (int i = 0; i < move_list.count; i++) {
        // MVV-LVA ordering
        int best_index = i;
        for (int j = i + 1; j < move_list.count; j++) {
            if (move_list.scores[j] > move_list.scores[best_index]) best_index = j;
        }

        std::swap(move_list[i], move_list[best_index]);
        std::swap(move_list.scores[i], move_list.scores[best_index]);

        Move move = move_list[i];
        board.make_move(move);
        if (!board.is_king_capturable()) {
            legal_moves_count++;
            int score = -quiescence(ply + 1, -beta, -alpha, board);
            board.unmake_move(move);

            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
        } else {
            board.unmake_move(move);
        }
    }

    if (in_check && legal_moves_count == 0) {
        return -INF + ply; // Mate detected inside QS
    }

    return alpha;
}

int negamax(int depth, int ply, Board& board, int alpha, int beta) {
    total_nodes++;

    if (depth == 0) {
        return quiescence(ply, alpha, beta, board);
    }

    int max = -INF;
    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list);
    board.score_moves(move_list);
    int legal_moves_count = 0;
    for (int i = 0; i < move_list.count; i++) {
        // MVV-LVA ordering
        int best_index = i;
        for (int j = i + 1; j < move_list.count; j++) {
            if (move_list.scores[j] > move_list.scores[best_index]) best_index = j;
        }

        std::swap(move_list[i], move_list[best_index]);
        std::swap(move_list.scores[i], move_list.scores[best_index]);

        Move move = move_list[i];
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

Move choose_negamax_move(int depth, Board& board) {
    total_nodes++;

    int max = -INF;
    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list);
    board.score_moves(move_list);
    Move bestMove;
    int ply = 0;
    for (int i = 0; i < move_list.count; ++i) {
        // MVV-LVA ordering
        int best_index = i;
        for (int j = i + 1; j < move_list.count; j++) {
            if (move_list.scores[j] > move_list.scores[best_index]) best_index = j;
        }

        std::swap(move_list[i], move_list[best_index]);
        std::swap(move_list.scores[i], move_list.scores[best_index]);

        Move move = move_list[i];
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
    Move get_best_move(const char* fen_str, int depth) {
        std::string_view fen{fen_str};
        // std::string_view initialfen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        // std::string_view testfen = "7k/8/8/7p/7q/8/4K3/1q1r4 w - - 12 68";

        std::println("FEN: {}", fen);

        Board board{fen};
        // Board board{testfen};

        auto start = std::chrono::high_resolution_clock::now();
        total_nodes = 0;

        Move bestmove = choose_negamax_move(depth, board);

        auto end = std::chrono::high_resolution_clock::now();
        double seconds = std::chrono::duration<double>(end - start).count();
        double nps = total_nodes / seconds;

        std::cout << "Depth: " << depth 
                << " | Nodes: " << total_nodes 
                << " | Time: " << seconds << "s" 
                << " | NPS: " << static_cast<uint64_t>(nps) << std::endl;

        std::cout << std::endl;

        // return 0;
        return bestmove;
    }
}

uint64_t perft(int depth, Board& board) { // For testing correctness of move generation and
    if (depth == 0) return 1ULL;

    uint64_t nodes = 0;
    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list);

    for (int i = 0; i < move_list.count; ++i) {
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
    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list);

    for (int i = 0; i < move_list.count; ++i) {
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
