//
// Created by Rongjie Yi on 2024/2/4 0004.
//

#ifndef MODELING_LLAMA_HPP
#define MODELING_LLAMA_HPP

#include "Layer.hpp"
#include "Module.hpp"
#include "configuration_llama.hpp"
#include "models/transformer/modeling_transformer.hpp"

using namespace mllm;

class LLaMAMLP final : public Module {
    Layer gate_proj;
    Layer silu;
    Layer up_proj;
    Layer down_proj;

public:
    LLaMAMLP() = default;
    LLaMAMLP(int hidden_dim, int mlp_hidden, const LLaMANameConfig &names, const string &base_name) {
        gate_proj = Linear(hidden_dim, mlp_hidden, false, base_name + names._gate_proj_name);
        silu = SiLU(base_name + "act");
        up_proj = Linear(hidden_dim, mlp_hidden, false, base_name + names._up_proj_name);
        down_proj = Linear(mlp_hidden, hidden_dim, false, base_name + names._down_proj_name);
    }
    vector<Tensor> Forward(vector<Tensor> inputs) override {
        auto x = gate_proj(inputs[0]);
        x = silu(x);
        auto y = up_proj(inputs[0]);
        x = x * y;
        x = down_proj(x);
        return {x};
    }
};

class LLaMABlock final : public Module {
    MultiHeadAttention attention;
    LLaMAMLP mlp;
    Layer norm1;
    Layer norm2;

public:
    LLaMABlock() = default;
    LLaMABlock(int hidden_dim, int head_size, int mlp_hidden, RoPEType RoPE_type, int cache_limit, const LLaMANameConfig &names, const string &base_name) {
        attention = MultiHeadAttention(hidden_dim, head_size, hidden_dim / head_size, RoPE_type, cache_limit, true, false, names, base_name + names._attn_base_name);
        mlp = LLaMAMLP(hidden_dim, mlp_hidden, names, base_name + names._ffn_base_name);
        norm1 = RMSNorm(hidden_dim, 1e-6, base_name + names._attn_norm_name);
        norm2 = RMSNorm(hidden_dim, 1e-6, base_name + names._ffn_norm_name);
    }
    vector<Tensor> Forward(vector<Tensor> inputs) override {
        auto x = norm1(inputs[0]);
        x = attention({x, x, x})[0];
        auto tmp = x + inputs[0];
        x = norm2(tmp);
        x = mlp({x})[0];
        x = x + tmp;
        return {x};
    }
};

class LLaMAModel final : public Module {
    Layer embedding;
    vector<LLaMABlock> blocks;
    Layer norm;
    Layer lm_head;

public:
    explicit LLaMAModel(const LLaMAConfig &config) :
        LLaMAModel(config.vocab_size, config.hidden_dim, config.head_size, config.mlp_hidden, config.block_num, config.RoPE_type, config.cache_limit,
                   config.names_config, config.names_config.blk_name) {
    }
    LLaMAModel(int vocab_size, int hidden_dim, int head_size, int mlp_hidden, int block_num, RoPEType RoPE_type, int cache_limit,
               const LLaMANameConfig &names, const string &base_name) {
        embedding = Embedding(vocab_size, hidden_dim, names.token_embd_name);
        blocks = List<LLaMABlock>(block_num, hidden_dim, head_size, mlp_hidden, RoPE_type, cache_limit, names, base_name);
        norm = RMSNorm(hidden_dim, 1e-6, names.post_norm_name);
        lm_head = Linear(hidden_dim, vocab_size, false, names.lm_head_name);
    }
    vector<Tensor> Forward(vector<Tensor> inputs) override {
        auto x = embedding(inputs[0]);
        for (auto &block : blocks) {
            x = block({x})[0];
        }
        x = norm(x);
        x = lm_head(x);
        return {x};
    }
};

#endif // MODELING_LLAMA_HPP