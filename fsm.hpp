#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <functional>
#include <cstring>

enum EdgeFlag
{
    E_NONE = 0,
    E_SILENT = 1 << 0,
    E_GLOBAL = 1 << 1,
};

template<typename State, typename Condition>
class FSM
{
private:
    struct Edge
    {
        State source;
        State destination;
        Condition simple_rule;
        std::function<bool(Condition)> rule;
        std::string label;
        EdgeFlag flags = E_NONE;

        Edge(State source, State destination, std::function<bool(Condition)> rule, std::string edge_label, EdgeFlag flags = E_NONE) : 
            source(source), destination(destination), rule(rule), flags(flags)
        {
            label = "";
            for (char sym : edge_label)
            {
                label += sym;
                if (sym == '\\')
                    label += sym;
            }
        }

        Edge(State source, State destination, Condition rule, std::string edge_label, EdgeFlag flags = E_NONE) : 
            source(source), destination(destination), rule(rule), flags(flags)
        {
            label = "";
            for (char sym : edge_label)
            {
                label += sym;
                if (sym == '\\')
                    label += sym;
            }
        }

        Edge() { }
    };

    State default_state;
    State current_state;
    State previous_state;

    std::unordered_set<State> possible_states;
    std::unordered_map<State, std::vector<Edge>> edges;
    std::vector<Edge> global_edges;
    std::unordered_map<State, std::string> state_names;

    bool getEdge(Condition cond, Edge& edge_ret)
    {
        for (Edge edge : edges[current_state])
        {
            if (edge.rule(cond))
            {
                edge_ret = edge;
                return true;
            }
        }

        for (Edge global_edge : global_edges)
        {
            if (global_edge.rule(cond))
            {
                edge_ret = global_edge;
                return true;
            }
        }

        return false;
    }

    void changeState(State destination)
    {
        // printf("Changing state from %d to %d\n", current_state, destination);
        current_state = destination;
    }

public:
    FSM(State default_state) : default_state(default_state), current_state(default_state), previous_state(default_state)
    {
        possible_states.insert(default_state);
    }

    FSM(State default_state, State start_state) : default_state(default_state), current_state(start_state), previous_state(start_state)
    {
        possible_states.insert(default_state);
        possible_states.insert(start_state);
    }


    void createEdge(State source, State destination, std::function<bool(Condition)> rule, std::string label, EdgeFlag flags = E_NONE)
    {
        if (edges.find(source) == edges.end())
            edges[source] = std::vector<Edge>();
        edges[source].push_back(Edge(source, destination, rule, label, flags));
        possible_states.insert(source);
        possible_states.insert(destination);
    }

    void createEdge(State source, State destination, Condition rule, std::string label, EdgeFlag flags = E_NONE)
    {
        if (edges.find(source) == edges.end())
            edges[source] = std::vector<Edge>();
        edges[source].push_back(Edge(source, destination, rule, label, flags));
        possible_states.insert(source);
        possible_states.insert(destination);
    }

    void createGlobalEdge(State destination, std::function<bool(Condition)> rule, std::string label, EdgeFlag flags = E_NONE)
    {
        global_edges.push_back(Edge(default_state, destination, rule, label, flags | E_GLOBAL));
        possible_states.insert(destination);
    }

    void createGlobalEdge(State destination, Condition rule, std::string label, EdgeFlag flags = E_NONE)
    {
        global_edges.push_back(Edge(default_state, destination, rule, label, flags | E_GLOBAL));
        possible_states.insert(destination);
    }

    void setStateName(State state, std::string_view state_name)
    {
        state_names[state] = state_name;
    }

    bool process(Condition cond)
    {
        Edge appropriate_edge;
        bool passed_edge = false;

        previous_state = current_state;
        if (getEdge(cond, appropriate_edge))
        {
            changeState(appropriate_edge.destination);
            passed_edge |= !(appropriate_edge.flags & E_SILENT == E_SILENT);
            if (current_state == default_state && getEdge(cond, appropriate_edge))
            {
                changeState(appropriate_edge.destination);
                passed_edge |= !(appropriate_edge.flags & E_SILENT == E_SILENT);
            }
        }

        return passed_edge;
    }

    void process(Condition cond, State& ended_state, bool& state_changed)
    {
        state_changed = process(cond);
        ended_state = previousState();
    }

    State previousState()
    {
        return previous_state;
    }

    State currentState()
    {
        return current_state;
    }

    std::string stateName(State state)
    {
        if (state_names.find(state) != state_names.end())
            return state_names[state];
        else
            return "";
    }

    void dumpFSMGraph(FILE* fno)
    {
        fprintf(fno, "digraph G {\n");

        for (auto state : possible_states)
        {
            fprintf(fno, "\t%d [shape=box label=\"", (int) state);

            if (state_names.find(state) != state_names.end())
                fprintf(fno, "%s (%d)", state_names[state].c_str(), (int) state);
            else
                fprintf(fno, "%d", (int) state);

            fprintf(fno, "\"]\n");
        }

        fprintf(fno, "\n");

        for (auto source : edges)
        {
            for (Edge edge : edges[source.first])
                fprintf(fno, "\t%d -> %d [style=%s label=\"%s\"]\n", 
                    (int) edge.source, (int) edge.destination, 
                    (edge.flags & E_SILENT == E_SILENT) ? "dotted" : "solid", 
                    edge.label.c_str()
                );
        }

        for (Edge global_edge : global_edges)
        {
            for (State source : possible_states)
            {
                fprintf(fno, "\t%d -> %d [style=%s label=\"%s\"]\n", 
                    (int) source, (int) global_edge.destination, 
                    (global_edge.flags & E_SILENT == E_SILENT) ? "dotted" : "solid", 
                    global_edge.label.c_str()
                );
            }
        }

        fprintf(fno, "}");
    }
};


template<typename State>
class TextFSM : public FSM<State, char>
{
    constexpr static char alphabet[] = 
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";

    static bool isLetter(char sym) {
        return (
            (sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z')
        );
    }

    static bool isNumber(char sym) {
        return (sym >= '0' && sym <= '9');
    }

    static bool isSpace(char sym) {
        return (sym == ' ' || sym == '\t');
    }

    static bool checkSymbol(const char* rule, const char sym)
    {
        bool direct = true;
        uintptr_t rule_length = strlen(rule);
        char previous_symbol = 0;
        for (uintptr_t idx = 0; idx < rule_length; idx++)
        {
            if (rule[idx] == '^')
                direct = false;

            else if (rule[idx] == '.')
                return direct;

            else if (rule[idx] == '-')
            {
                idx++;
                bool inside = false;
                uintptr_t alph_length = strlen(alphabet);
                for (uintptr_t alph_idx = 0; alph_idx < alph_length; alph_idx++)
                {
                    if (alphabet[alph_idx] == previous_symbol)
                        inside = true;

                    if (inside == true && sym == alphabet[alph_idx])
                        return direct;
                    
                    if (alphabet[alph_idx] == rule[idx])
                        break;
                }
            }

            else if (rule[idx] == '\\')
                switch (rule[++idx])
                {
                    case '\\':
                        if (sym == '\\')
                            return direct;
                        break;
                    case '^':
                        if (sym == '^')
                            return direct;
                        break;
                    case '-':
                        if (sym == '-')
                            return direct;
                        break;
                    case '.':
                        if (sym == '.')
                            return direct;
                        break;
                    case 'w':
                        if (isLetter(sym))
                            return direct;
                        break;
                    case 'd':
                        if (isNumber(sym))
                            return direct;
                        break;
                    case 's':
                        if (isSpace(sym))
                            return direct;
                        break;
                    case 'n':
                        if (sym == '\n')
                            return direct;
                        break;
                    case 't':
                        if (sym == '\t')
                            return direct;
                        break;
                    case '0':
                        if (sym == '\0')
                            return direct;
                        break;
                }

            else if (rule[idx] == sym)
                return direct;

            previous_symbol = rule[idx];
        }

        return !direct;
    }

public:
    TextFSM(State default_state) : FSM<State, char>(default_state) { }
    TextFSM(State default_state, State start_state) : FSM<State, char>(default_state, start_state) { }

    void createEdge(State source, State destination, const char* rule, EdgeFlag flags = E_NONE)
    {
        std::function<bool(char)> rule_function = [rule](char sym) {
            return checkSymbol(rule, sym);
        };

        FSM<State, char>::createEdge(source, destination, rule_function, rule, flags);
    }

    void createEdge(State source, State destination, const char rule, EdgeFlag flags = E_NONE)
    {
        FSM<State, char>::createEdge(source, destination, rule, rule, flags);
    }

    void createGlobalEdge(State destination, const char* rule, EdgeFlag flags = E_NONE)
    {
        std::function<bool(char)> rule_function = [rule](char sym) {
            return checkSymbol(rule, sym);
        };

        FSM<State, char>::createGlobalEdge(destination, rule_function, rule, flags);
    }

    void createGlobalEdge(State destination, const char rule, EdgeFlag flags = E_NONE)
    {
        FSM<State, char>::createGlobalEdge(destination, rule, rule, flags);
    }
};
