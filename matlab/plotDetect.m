close all;
clear;
BER_ideal = importdata('statistic\BER_ideal.txt','%s');
BER_ideal_pilot = importdata('statistic\BER_ideal_pilot.txt','%s');
BER_detect = importdata('statistic\BER_detect.txt','%s');
SNR = 0:50;

lineWidth=1.5;
for i = 1:15

	figure;
	semilogy(SNR,BER_ideal((i-1)*51+1:i*51),'LineWidth',lineWidth);
	hold on;
	semilogy(SNR,BER_ideal_pilot((i-1)*51+1:i*51),'LineWidth',lineWidth);
	semilogy(SNR,BER_detect((i-1)*51+1:i*51),'LineWidth',lineWidth);
	legend('���������ŵ�',...
		'���뵼Ƶ�ŵ�',...
		'�����ŵ�'...
		);
	xlabel('Eb/N0(dB)');
	ylabel('BER');
	title(['ѡ��ͬ�����ŵ�����������(numFlow=8,CQI=',num2str(i),')']);
	grid on;

end

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