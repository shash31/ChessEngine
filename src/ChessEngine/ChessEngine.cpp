#include <emscripten.h>
#include <chrono>
#include "tt.h"
#include "Board.h"
#include "eval.h"

// TODOs:
// - Put site link on resume and update and apply to jobs
// - Implement transposition table
// - Implement iterative deepening
// - Maybe implement killer move heuristic
// - Maybe implement history heuristic
// - Look into improving search (PVS, NegaScout, etc.)
// - Look into using NNUE as evaluation instead
// - Look into multithreading with web workers

constexpr int INF = 1e9;

uint64_t total_nodes;

TranspositionTable tt;

// Time limit for iterative deepening
std::chrono::time_point<std::chrono::steady_clock> start_time;
int allocated_time_ms;
bool stopped;

inline void check_time() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    if (elapsed >= allocated_time_ms) {
        stopped = true;
    }
}

int quiescence(int ply, int alpha, int beta, Board& board) {
    total_nodes++;

    // Check time every 2048 nodes
    if ((total_nodes & 2047) == 0) check_time();

    // If time expired, abort search immediately
    if (stopped) return 0;

    int original_alpha = alpha;
    Move tt_move = 0;
    int tt_score = 0;

    if (tt.probe(board.hash, 0, alpha, beta, tt_score, tt_move, ply)) return tt_score;

    Square king_pos = static_cast<Square>(std::countr_zero(board.piece_type_bb[board.turn][KING]));
    bool in_check = board.is_square_attacked(king_pos);

    int stand_pat = -INF;
    if (!in_check) {
        stand_pat = pesto_eval(board);
        if (stand_pat >= beta) return beta;
        if (alpha < stand_pat) alpha = stand_pat;
    }

    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list, !in_check); // ALL moves if in check, ONLY captures if quiet
    board.score_moves(move_list, tt_move);

    int legal_moves_count = 0;
    Move best_move = 0;
    int best_score = in_check ? -INF : stand_pat;

    for (int i = 0; i < move_list.count; i++) {
        // Selection sort inline
        int best_index = i;
        for (int j = i + 1; j < move_list.count; j++) {
            if (move_list.scores[j] > move_list.scores[best_index]) best_index = j;
        }
        std::swap(move_list[i], move_list[best_index]);
        std::swap(move_list.scores[i], move_list.scores[best_index]);

        Move move = move_list[i];

        // Delta Pruning: Skip bad captures if not in check and can't reach alpha
        if (!in_check && ((move >> 12) < 8)) { // If not in check & not a promotion
            int captured_val = material_values[char_to_piece(board.grid[(move >> 6) & 0x3F])];
            if (stand_pat + captured_val + 200 < alpha) continue;
        }

        board.make_move(move);
        if (!board.is_king_capturable()) {
            legal_moves_count++;
            int score = -quiescence(ply + 1, -beta, -alpha, board);
            board.unmake_move(move);

            if (score > best_score) {
                best_score = score;
                best_move = move;

                if (score > alpha) alpha = score;
                if (alpha >= beta) break; // Cutoff
            }
        } else {
            board.unmake_move(move);
        }
    }

    if (in_check && legal_moves_count == 0) {
        return -INF + ply; // Actual Checkmate
    }

    TTFlag flag = (best_score <= original_alpha) ? UPPER : (best_score >= beta ? LOWER : EXACT);
    tt.store(board.hash, 0, best_score, flag, best_move, ply);

    return best_score;
}

int negamax(int depth, int ply, Board& board, int alpha, int beta) {
    total_nodes++;

    // Check time every 2048 nodes
    if ((total_nodes & 2047) == 0) check_time();

    // If time expired, abort search immediately
    if (stopped) return 0;

    int original_alpha = alpha;
    Move tt_move = 0;
    int tt_score = 0;

    if (tt.probe(board.hash, depth, alpha, beta, tt_score, tt_move, ply)) return tt_score;

    if (depth == 0) return quiescence(ply, alpha, beta, board);

    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list);
    board.score_moves(move_list, tt_move);
    
    int best_score = -INF;
    Move best_move = 0;
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
            if (score > best_score) {
                best_score = score;
                best_move = move;
            }
            alpha = std::max(best_score, alpha);
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

    TTFlag flag;
    if (best_score <= original_alpha) {
        flag = UPPER; // Fail-low
    } else if (best_score >= beta) {
        flag = LOWER; // Fail-high
    } else {
        flag = EXACT; // PV Node
    }

    tt.store(board.hash, depth, best_score, flag, best_move, ply);

    return best_score;
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    Move get_best_move(const char* fen_str, int maxTime, bool initial) {
        if (initial) {
            tt.resize(64); // 64 MB
            zobrist_keys.init();
            init_tables();

            std::println("Allocated space for Transposition Table");
            std::println("Initialized Zobrist keys");
            std::println("Initialized attack tables");

            return 0;
        }

        std::string_view fen{fen_str};
        // std::string_view initialfen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        // std::string_view testfen = "rnbqkb1r/pppppppp/8/4n3/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 1";

        std::println("FEN: {}", fen);

        Board board{fen};
        // Board board{initialfen};
        // Board board2{testfen};

        current_age++;

        std::println("Current age: {}", current_age);

        // Iterative Deepening Search
        start_time = std::chrono::steady_clock::now();
        allocated_time_ms = maxTime*1000;
        stopped = false;
        double totalTime = 0;
        
        uint64_t all_nodes = 0;

        Move bestmove;
        int alpha = -INF;
        int beta = INF;

        for (int depth = 1; depth < 30; depth++) { 
            auto start = std::chrono::steady_clock::now();
            total_nodes = 0;
            
            int score = negamax(depth, 0, board, alpha, beta);

            if (stopped) break; // Time ran out; Discard results

            // Retrieve best move found so far from TT for the root position
            Move tt_move = 0;
            int dummy_score = 0;
            tt.probe(board.hash, depth, alpha, beta, dummy_score, tt_move, 0);
            
            if (tt_move != 0) bestmove = tt_move;

            auto end = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration<double>(end - start).count();
            totalTime += seconds;
            double nps = total_nodes / seconds;
            all_nodes += total_nodes;

            std::println("Score: {}", score);
            std::println("Best move: {}", moveToString(bestmove));

            std::cout << "Depth: " << depth 
                    << " | Nodes: " << total_nodes 
                    << " | Time: " << seconds << "s" 
                    << " | NPS: " << static_cast<uint64_t>(nps) << std::endl;

            // Stop early if a forced mate is found
            if (score > 90000 || score < -90000) {
                break;
            }
        }

        std::println("Total time: {}", totalTime);
        std::println("Total nodes: {}", all_nodes);
        std::println("NPS: {}", static_cast<uint64_t>(all_nodes / totalTime));

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
