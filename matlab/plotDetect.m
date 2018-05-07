close all;
clear;
BER_ideal = importdata('BER_ideal.txt','%s');
BER_ideal_pilot = importdata('BER_ideal_pilot.txt','%s');
BER_detect = importdata('BER_detect.txt','%s');
SNR = 0:50;

figure;
lineWidth=1.5;
semilogy(SNR,BER_ideal,'LineWidth',lineWidth);
hold on;
semilogy(SNR,BER_ideal_pilot,'LineWidth',lineWidth);
semilogy(SNR,BER_detect,'LineWidth',lineWidth);
legend('���������ŵ�',...
	'���뵼Ƶ�ŵ�',...
	'�����ŵ�'...
	);
xlabel('Eb/N0(dB)');
ylabel('BER');
title('ѡ��ͬ�����ŵ�����������(numFlow=8,CQI=15)');
grid on;