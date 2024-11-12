#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>
#include <memory>

// Variáveis globais de sincronização
constexpr int NUM_JOGADORES = 4;
std::unique_ptr<std::counting_semaphore<NUM_JOGADORES>> cadeira_sem = std::make_unique<std::counting_semaphore<NUM_JOGADORES>>(NUM_JOGADORES - 1);
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true}; 

int tempoVariavel(int min, int max) {
    std::random_device numero;
    std::mt19937 gerar(numero());
    std::uniform_int_distribution<> distrubuicao(min, max);
    return distrubuicao(gerar);
}

class JogoDasCadeiras {
public:
    int num_jogadores;
    int cadeiras;

    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {}

    void iniciar_rodada() {
        // Inicia uma nova rodada, liberando permissões e atualizando o semáforo
        cadeira_sem->release(cadeiras);
        num_jogadores--;
        cadeiras--;
        exibir_estado();
        cadeira_sem.reset(new std::counting_semaphore<NUM_JOGADORES>(cadeiras));
    }

    void parar_musica() {
        // Para a música e notifica os jogadores para tentar se sentar
        std::cout << "> A música parou! Os jogadores estão tentando se sentar..." << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        musica_parada = true;
        music_cv.notify_all();
    }

    void eliminar_jogador(int id) {
        while (true) {
            std::cout << "O Jogador numero " << id << (jogo_ativo ? " foi eliminado!" : " foi o vencedor!") << "\n\n";
            break;
        }
    }
    
    void exibir_estado() {
        // TODO: Exibe o estado atual das cadeiras e dos jogadores
        std::cout << "-------------------------------------------" << std::endl
            << "Nova Rodada " << std::endl 
            << "Número de jogadores atuais: " << num_jogadores << std::endl 
            << "Número de cadeiras disponíveis: " << cadeiras << std::endl 
            << "-------------------------------------------" << std::endl;
    }
};

class Jogador {
public:
    int id;
    JogoDasCadeiras& jogo;
    bool eliminado;

    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo), eliminado(false) {}

    void tentar_ocupar_cadeira() {
    
        std::unique_lock<std::mutex> lock(music_mutex);
        music_cv.wait(lock);
        
        if (eliminado) return;
        enum Estado { TENTANDO, ELIMINADO };
    
        Estado estado = cadeira_sem->try_acquire() ? TENTANDO : ELIMINADO;
    
        switch (estado) {
            case TENTANDO:
                std::cout << "O Jogador Número " << id << " conseguiu uma cadeira!" << std::endl;
                break;
    
            case ELIMINADO:
                eliminado = true;
                jogo.eliminar_jogador(id);
                break;
        }
    }
    void joga() {
        while (!eliminado) {
            if (jogo.cadeiras > 0) {
                tentar_ocupar_cadeira();
            }
        }
        if (jogo.cadeiras == 0) {
            std::cout << "O Jogador número " << id << " foi o vencedor!" << std::endl;
        }
    }
    
};

class Coordenador {
public:
    JogoDasCadeiras& jogo;

    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        
        while (jogo.cadeiras > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            jogo.parar_musica();
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            jogo.iniciar_rodada();
        }

        jogo_ativo = false;
        music_cv.notify_all(); 
    }
};

// Função principal
int main() {
    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::thread> jogadores;

    // Criação das threads dos jogadores
    std::vector<Jogador> jogadores_objs;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_objs.emplace_back(i, jogo);
    }

    for (int i = 0; i < NUM_JOGADORES; ++i) {
        jogadores.emplace_back(&Jogador::joga, &jogadores_objs[i]);
    }

    // Thread do coordenador
    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);

    // Espera pelas threads dos jogadores
    for (auto& t : jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Espera pela thread do coordenador
    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }

    std::cout << "Jogo das Cadeiras finalizado." << std::endl;
    return 0;
}