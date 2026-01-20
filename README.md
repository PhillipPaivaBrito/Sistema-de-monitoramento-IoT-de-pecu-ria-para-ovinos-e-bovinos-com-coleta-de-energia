# Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia
Esse repositório existe apenas para mostrar o desenvolvimento do meu Trabalho de conclusão em curso em engenharia da computação, o código não tera atualizações futuras aqui, caso queira ver futuras atualizações, entrar em contato para acessar o repositório principal. o software foi desenvolvido utilizando o platformio no vscode ainda no Windows 10, então pode ser que precise de ajustes dependendo do SO que esteja utilizando, já o harware foi desenvolvido no KiCad. As imagens a seguir mostram o fluxograma simplificado do sistema e o fluxograma especifico do transmissor e receptor respectivamente.

![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Fluxograma%20do%20transmissor%20e%20do%20receptor.jpg)
![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Fluxograma%20do%20software%20do%20transmissor.png)
![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Fluxograma%20do%20transmissor%20e%20do%20receptor.png)

Caso queira testar o dashboard, você precisara instalar o NodeRed(https://nodered.org/) e importar as configurações do arquivo flows.json, não esqueça de ajustar o MQTT para o broker de sua escolha e instalar a biblioteca do maps(https://flows.nodered.org/node/node-red-contrib-web-worldmap) a imagens a seguir mostra como deve parecer o flow e em seguida o dashboard.

![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Implementa%C3%A7%C3%A3o%20do%20Node%20RED.png)
![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Painel%20do%20dashboard.png)

O broker que utilizei foi o mosquitto(https://mosquitto.org/) que é open source, porém qualquer broker pode ser utilizado desde que seja configurado corretamente.

A seguir estão imagens do desenvolvimento do hardware transmissor.
![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Projeto%20na%20Protoboard.png)
![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Projeto%20da%20placa%20no%20Kicad.png)
![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Placa%20de%20circuito%20impresso.png)
![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Parte%20inferior%20com%20painel.jpg)
![image alt](https://github.com/PhillipPaivaBrito/Sistema-de-monitoramento-IoT-de-pecu-ria-para-ovinos-e-bovinos-com-coleta-de-energia/blob/39e89aaf3d64fe7e2538c85453d5e34461b347c1/imagens/Face%20superior%20da%20PCB.jpg)
