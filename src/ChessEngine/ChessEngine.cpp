#include <emscripten.h>
#include "Board.h"
#include "eval.h"

// TODOs:
// 
// - Set up web workers potentially
//
// - Look into using NNUE as evaluation instead of own evaluation function

constexpr int INF = 1e9;

// float getMaterialCount(Color side, Board& board) {
//     float material = 0;

//     material += std::popcount(board.piece_type_bb[side][QUEEN])*9;
//     material += std::popcount(board.piece_type_bb[side][ROOK])*5;
//     material += std::popcount(board.piece_type_bb[side][BISHOP])*3;
//     material += std::popcount(board.piece_type_bb[side][KNIGHT])*3;
//     material += std::popcount(board.piece_type_bb[side][PAWN]);

//     return material;
// }

// float evaluate(Board& board) {
//     float who2move = board.turn == WHITE ? 1 : -1;
//     return (getMaterialCount(WHITE, board) - getMaterialCount(BLACK, board)) * who2move;
// }

int quiescence(int alpha, int beta, Board& board) {
    // int stand_pat = evaluate(board);
    int stand_pat = pesto_eval(board);
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    std::vector<Move> capture_moves = board.generate_pseudo_legal_moves(true); // Captures only = true;
    
    for (const Move& move : capture_moves) {
        if ((move >> 12) == 0) {
            std::println("FLAG 0 IN CAPTURES");
            board.printAllBoards();
            std::println("FLAG 0 IN CAPTURES END");
        }
        board.make_move(move);
        if (!board.is_king_capturable()) {
            int score = -quiescence(-beta, -alpha, board);
            board.unmake_move(move);

            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
        } else {
            board.unmake_move(move);
        }
    }
    return alpha;
}

int negamax(uint8_t depth, Board& board, int alpha, int beta) {
    if (depth == 0) {
        return quiescence(alpha, beta, board);
    }

    int max = -INF;
    std::vector<Move> moves = board.generate_pseudo_legal_moves();
    uint8_t legal_moves_count = 0;
    for (Move move : moves) {
        board.make_move(move);
        if (!board.is_king_capturable()) {
            legal_moves_count++;
            int score = -negamax(depth - 1, board, -beta, -alpha);
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
            return -INF + depth; 
        }
        return 0; // stalemate
    }

    return max;
}

Move choose_negamax_move(uint8_t depth, Board& board) {
    if (depth == 0) return pesto_eval(board);

    int max = -INF;
    std::vector<Move> moves = board.generate_pseudo_legal_moves();
    Move bestMove;
    for (Move move : moves) {
    // Move move = moves[0];
        board.make_move(move);
        if (!board.is_king_capturable()) {
            // std::println("Printing in choose_negamax: ");
            printMove(move);
            int score = -negamax(depth - 1, board, -INF, INF);
            std::println("Score: {}", score);
            if (score > max) {
                max = score;
                bestMove = move;
            }
        }
        board.unmake_move(move);
    }

    // board.printAllBoards();
    // std::println("Current eval: {}", evaluate(board));
    std::println("Current pesto_eval for {}: {}", board.turn == WHITE ? "white" : "black", pesto_eval(board));
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
        // std::string_view testfen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1";
        // std::string_view testfen2 = "rnbqkbnr/pppppppp/8/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1";
        // std::string_view testfen3 = "rnbqkbnr/pppppppp/8/8/8/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1";
        // std::string_view testfen4 = "rnbqkbnr/pppppppp/8/8/8/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1";
        // std::string_view testfen5 = "rnbqkbnr/pppppppp/8/8/8/8/1PPPPPPP/RNBQKBNR b KQkq - 0 1";

        if (engine_initialized) std::println("Lookup tables generated");;

        std::println("{}", fen);

        Board board{fen};
        // Board board{initialfen};
        // Board board2{testfen};
        // Board board3{testfen2};
        // Board board4{testfen3};
        // Board board5{testfen4};

        // std::println("Initial board PESTO Evaluation: {}", pesto_eval(board));
        // std::println("Board2 PESTO Evaluation: {}", pesto_eval(board2));
        // std::println("Board3 PESTO Evaluation: {}", pesto_eval(board3));
        // std::println("Board4 PESTO Evaluation: {}", pesto_eval(board4));
        // std::println("Board5 PESTO Evaluation: {}", pesto_eval(board5));

        // Random moves for now
        // std::vector<Move> moves = board.generate_pseudo_legal_moves();
        // std::vector<Move> legal_moves;
        // std::println("Moves:");
        // for (Move move: moves) {
        //     board.make_move(move);
        //     if (!board.is_king_capturable()) {
        //         legal_moves.push_back(move);
        //         printMove(move);
        //     }
        //     board.unmake_move(move);
        // }
        // std::println("Size: {}", legal_moves.size());
        
        // std::random_device rd;  
        // std::mt19937 gen(rd()); 

        // std::uniform_int_distribution<std::size_t> dist(0, legal_moves.size() - 1);

        // std::size_t rnd = dist(gen);

        // std::string move = moveToString(legal_moves[rnd]);

        std::println("Running at depth {}", depth);
        static std::string move_buffer;
        move_buffer = moveToString(choose_negamax_move(depth, board));
        // move_buffer = "e2e4";
        std::println("Move: {}", move_buffer);

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

void perft_divide(int depth, Board& board) {
    uint64_t total_nodes = 0;
    std::vector<Move> move_list = board.generate_pseudo_legal_moves();

    for (int i = 0; i < move_list.size(); ++i) {
        Move move = move_list[i];

        board.make_move(move);
        if (board.is_king_capturable()) {
            std::println("ran into illegal move");
            std::println("turn: {}; oppSide: {}", static_cast<int>(board.turn), static_cast<int>(board.oppSide));
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
