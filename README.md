# First-Person Aim Sensitivity (mod para o Dusklight)

Adiciona controles de **sensibilidade da mira em primeira pessoa**:

- Arco
- Estilingue
- Gancho (Clawshot) e Gancho duplo (Double Clawshots), que **compartilham o mesmo controle**

Nada mais é alterado (bumerangue, bola e corrente etc. continuam normais). Funciona com analógico,
mouse, giroscópio e toque, porque o ajuste é feito no ponto onde o jogo junta todas essas entradas.

## Como usar no jogo

1. Coloque o arquivo `aim_sensitivity.dusk` na pasta de mods (veja abaixo) ou arraste-o para a janela do Dusklight.
2. Abra o menu **Mods**, ative **First-Person Aim Sensitivity**.
3. No painel do mod, ajuste **Bow**, **Slingshot** e **Clawshot / Double Clawshots**, de 10% a 300%, de 5 em 5. **100% = jogo original.**

A configuração fica salva no `config.json` do Dusklight e pode ser mudada com o jogo aberto.

## Por que não deve dar conflito com outros mods

- Usa só *hooks* `pre` e `post` na função `daAlink_c::setBodyAngleToCamera` (nunca "replace"), então outros mods podem mexer na mesma função.
- Roda por último na cadeia de hooks (prioridade baixa), então escala também o que outros mods tiverem feito na mira naquele frame.
- Em 100% (padrão) os hooks saem na hora: comportamento idêntico ao original.
- Só age com Arco, Estilingue, Gancho ou Gancho duplo **e** com a câmera em primeira pessoa.
- Mantém os mesmos limites de inclinação do jogo (a mira nunca passa do que o original permite, mesmo a 300%).
- Se o hook não puder ser instalado (versão do Dusklight diferente), o mod se desativa sozinho e mostra o erro, sem afetar o jogo.
- Pode ser ativado, desativado e recarregado em tempo real; nada fica preso no jogo depois de desativar.

## Como compilar

Mods nativos precisam ser compilados **para a mesma versão do Dusklight que você usa**. O projeto está
configurado para a **v2.0.3**. Se você usa outra, troque a linha `set(DUSKLIGHT_VERSION "v2.0.3")` no
`CMakeLists.txt` pela sua versão (tag como `v2.0.2`).

### Opção A: GitHub Actions (mais fácil, gera para todas as plataformas)

1. Crie um repositório no GitHub e envie o conteúdo desta pasta.
2. Aba **Actions**: espere o workflow **Build** terminar.
3. Baixe o artefato **mod-combined**: dentro está o `aim_sensitivity.dusk` pronto.

### Opção B: compilar no Windows

Precisa de: Visual Studio Build Tools (carga de trabalho "Desenvolvimento para desktop com C++"), CMake, Git e Ninja.
Abra o **x64 Native Tools Command Prompt for VS**, entre nesta pasta e rode:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

O resultado fica em `build\mods\aim_sensitivity.dusk`. (Um `.dusk` compilado localmente só vale para o seu sistema.)

## Onde instalar o .dusk

- Windows: `%APPDATA%\TwilitRealm\Dusklight\mods`
- Linux: `~/.local/share/TwilitRealm/Dusklight/mods`
- macOS: `~/Library/Application Support/TwilitRealm/Dusklight/mods`

Durante testes: recompile, copie o `.dusk` e clique em **Reload** no gerenciador de mods do jogo.

## Ajustes no código (`src/mod.cpp`)

- `kMinPercent` / `kMaxPercent`: limites do controle.
- `g_items`: a lista de itens suportados. Cada linha tem o item (e, opcionalmente, um segundo item que divide o mesmo controle), o nome da configuração e o texto mostrado no painel. Para incluir outro item, basta adicionar uma linha.
- Os textos da interface estão em inglês de propósito, para combinar com o resto do menu do Dusklight.
