import json
import numpy as np
import re
from typing import List, Dict, Any

class MemConfigGenerator:
    def __init__(self, config_file: str = "config.json"):
        self.config_file = config_file
        self.config_data = self.load_config()
        self.bank_functions = self.parse_bank_functions()
        self.matrix_size = 30  # Total matrix size: 30x30 as in original C++ code
        
        # Calculate configuration parameters
        self.num_bank_func = len(self.bank_functions)  # Number of bank functions
        self.num_col_bits = 13  # Fixed number of column bits
        self.num_row_bits = self.matrix_size - self.num_bank_func - self.num_col_bits  # Remaining bits for row
    
    def load_config(self) -> Dict[str, Any]:
        """Load configuration values from JSON file"""
        try:
            with open(self.config_file, 'r') as f:
                config = json.load(f)
            print(f"✅ Configuration loaded from {self.config_file}")
            return config
        except FileNotFoundError:
            print(f"❌ Configuration file {self.config_file} not found")
            return {}
        except json.JSONDecodeError:
            print(f"❌ Invalid JSON in {self.config_file}")
            return {}
    
    def parse_bank_functions(self) -> List[List[int]]:
        """Parse bank functions from RE.log file"""
        reverse_file = self.config_data.get('reverse', '../reverse/output/RE.log')
        bank_functions = []
        
        try:
            with open(reverse_file, 'r') as f:
                content = f.read()
            
            # Find the bank function section
            pattern = r'\(\s*([\d,\s]+)\s*\)'
            
            # Filter to only include the 5 bank functions after "=== The 5 bank function ==="
            bank_section_start = content.find("=== The 5 bank function ===")
            if bank_section_start != -1:
                bank_section = content[bank_section_start:]
                bank_matches = re.findall(pattern, bank_section)
                
                for match in bank_matches[:5]:  # Take first 5 matches after the header
                    numbers = [int(x.strip()) for x in match.split(',')]
                    bank_functions.append(numbers)
            
            print(f"✅ Bank functions parsed from {reverse_file}")
            print("Bank functions found:")
            for i, func in enumerate(bank_functions):
                print(f"  Bank {i}: {func}")
                
        except FileNotFoundError:
            print(f"❌ Reverse file {reverse_file} not found")
            # Fallback to default bank functions
            bank_functions = [
                [16, 20, 23, 24, 27, 30, 33],
                [14, 18, 26, 29, 32],
                [17, 21, 22, 25, 28, 31],
                [15, 19],
                [9, 11, 13]
            ]
            print("⚠ Using default bank functions")
        
        return bank_functions
    
    def get_config_value(self, key: str, default: int = 0) -> int:
        """Get configuration value with fallback to default"""
        if self.config_data and key in self.config_data:
            return self.config_data[key]
        else:
            print(f"⚠ Using default value for {key}: {default}")
            return default
    
    def calculate_shift_mask(self) -> Dict[str, int]:
        """Calculate shift and mask values based on configuration"""
        # Get configuration values
        rank_value = self.get_config_value('RANK', 2)
        bankgroup_value = self.get_config_value('BANKGROUP', 4)
        bank_value = self.get_config_value('BANK', 4)
        
        # Calculate bit widths
        rank_bits = (rank_value - 1).bit_length() if rank_value > 1 else 1
        bankgroup_bits = (bankgroup_value - 1).bit_length() if bankgroup_value > 1 else 1
        bank_bits = (bank_value - 1).bit_length() if bank_value > 1 else 1
        
        print(f"=== Bit Width Calculation ===")
        print(f"RANK: {rank_value} -> {rank_bits} bits")
        print(f"BANKGROUP: {bankgroup_value} -> {bankgroup_bits} bits")
        print(f"BANK: {bank_value} -> {bank_bits} bits")
        
        # Calculate shifts (from LSB to MSB)
        col_shift = 0
        row_shift = col_shift + self.num_col_bits
        bank_shift = row_shift + self.num_row_bits
        bankgroup_shift = bank_shift + bank_bits
        rank_shift = bankgroup_shift + bankgroup_bits
        
        print(f"=== Shift Calculation ===")
        print(f"COL_SHIFT: {col_shift} (0 + 0)")
        print(f"ROW_SHIFT: {row_shift} ({col_shift} + {self.num_col_bits})")
        print(f"BK_SHIFT: {bank_shift} ({row_shift} + {self.num_row_bits})")
        print(f"BG_SHIFT: {bankgroup_shift} ({bank_shift} + {bank_bits})")
        print(f"RK_SHIFT: {rank_shift} ({bankgroup_shift} + {bankgroup_bits})")
        
        # Calculate masks
        col_mask = (1 << self.num_col_bits) - 1
        row_mask = (1 << self.num_row_bits) - 1
        bank_mask = (1 << bank_bits) - 1
        bankgroup_mask = (1 << bankgroup_bits) - 1
        rank_mask = (1 << rank_bits) - 1
        
        print(f"=== Mask Calculation ===")
        print(f"COL_MASK: {col_mask} (2^{self.num_col_bits} - 1)")
        print(f"ROW_MASK: {row_mask} (2^{self.num_row_bits} - 1)")
        print(f"BK_MASK: {bank_mask} (2^{bank_bits} - 1)")
        print(f"BG_MASK: {bankgroup_mask} (2^{bankgroup_bits} - 1)")
        print(f"RK_MASK: {rank_mask} (2^{rank_bits} - 1)")
        
        return {
            "SC_SHIFT": 0,
            "SC_MASK": 0,
            "RK_SHIFT": rank_shift,
            "RK_MASK": rank_mask,
            "BG_SHIFT": bankgroup_shift,
            "BG_MASK": bankgroup_mask,
            "BK_SHIFT": bank_shift,
            "BK_MASK": bank_mask,
            "ROW_SHIFT": row_shift,
            "ROW_MASK": row_mask,
            "COL_SHIFT": col_shift,
            "COL_MASK": col_mask
        }
    
    def calculate_identifier(self) -> int:
        """Calculate IDENTIFIER value based on macro definitions and config file"""
        # Get values from configuration file
        config_values = {
            'CHAN': self.get_config_value('CHAN', 1),
            'DIMM': self.get_config_value('DIMM', 1),
            'RANK': self.get_config_value('RANK', 2),
            'BANKGROUP': self.get_config_value('BANKGROUP', 4),
            'BANK': self.get_config_value('BANK', 4),
            'SAMSUNG': self.get_config_value('SAMSUNG', 1)
        }
        
        # Each field uses 4-bit multiples as shift positions
        shifts = {
            'SAMSUNG': 4 * 5,   # 20 bits (4*5)
            'CHAN': 4 * 4,      # 16 bits (4*4)
            'DIMM': 4 * 3,      # 12 bits (4*3)
            'RANK': 4 * 2,      # 8 bits (4*2)
            'BANKGROUP': 4 * 1, # 4 bits (4*1)
            'BANK': 4 * 0       # 0 bits (4*0)
        }
        
        identifier = 0
        print("=== IDENTIFIER Calculation ===")
        for field, shift in shifts.items():
            value = config_values[field]
            field_value = value << shift
            identifier |= field_value
            print(f"{field:10} = {value} << {shift:2} = {field_value:10} (0x{field_value:08X})")
        
        return identifier
    
    def generate_dram_matrix(self) -> np.ndarray:
        """Generate 30x30 DRAM matrix with correct structure and bit order"""
        dram_matrix = np.zeros((self.matrix_size, self.matrix_size), dtype=int)
        
        print("=== Generating DRAM Matrix ===")
        print(f"Matrix size: {self.matrix_size}x{self.matrix_size}")
        print(f"Bank functions: 0 - {self.num_bank_func - 1}")
        print(f"Row bits: {self.num_bank_func} - {self.num_bank_func + self.num_row_bits - 1}") 
        print(f"Column bits: {self.num_bank_func + self.num_row_bits} - {self.matrix_size - 1}")
        
        # Bank functions - set bits for each bank function row
        for row_idx, bank_func in enumerate(self.bank_functions):
            adjusted_bits = [bit for bit in bank_func if bit < self.matrix_size]
            for bit_pos in adjusted_bits:
                dram_matrix[row_idx, self.matrix_size - 1 - bit_pos] = 1
        
        # Row bits - set bits from position 0 to 11
        for i in range(self.num_row_bits):
            bit_pos = i 
            dram_matrix[self.num_bank_func + i, bit_pos] = 1
        
        # Column bits - set bits from position 29 down to 17
        for i in range(self.num_col_bits):
            bit_pos = self.matrix_size - 1 - i
            dram_matrix[self.matrix_size - i -1, bit_pos] = 1
            
        return dram_matrix
    
    def gf2_inv(self, matrix: np.ndarray) -> np.ndarray:
        """Calculate inverse of a matrix in GF(2) using Gaussian elimination"""
        n = matrix.shape[0]
        # Create augmented matrix [A | I]
        augmented = np.hstack([matrix, np.eye(n, dtype=int)])
        
        for i in range(n):
            # Find pivot row
            pivot = np.where(augmented[i:, i] == 1)[0]
            if len(pivot) == 0:
                raise ValueError("Matrix is not invertible in GF(2)")
            pivot = pivot[0] + i
            
            # Swap rows if necessary
            if pivot != i:
                augmented[[i, pivot]] = augmented[[pivot, i]]
            
            # Eliminate other rows
            for j in range(n):
                if j != i and augmented[j, i] == 1:
                    augmented[j] ^= augmented[i]
        
        # Extract inverse matrix from the right half
        return augmented[:, n:]
    
    def generate_addr_matrix(self, dram_matrix: np.ndarray) -> np.ndarray:
        """Generate inverse matrix using GF(2) Gaussian elimination"""
        print("\n=== Calculating Inverse Matrix ===")
        
        try:
            # Compute inverse in GF(2) using Gaussian elimination
            addr_matrix = self.gf2_inv(dram_matrix)
            print("✅ GF(2) inverse successful")
            
            # Verify inverse matrix property
            identity_check = (dram_matrix @ addr_matrix) % 2
            is_identity = np.array_equal(identity_check, np.eye(self.matrix_size, dtype=int))
            print(f"✅ Inverse verification: {'PASS' if is_identity else 'FAIL'}")
            
            return addr_matrix
            
        except ValueError as e:
            print(f"❌ GF(2) inverse failed: {e}")
            print("⚠ Falling back to identity matrix")
            return np.eye(self.matrix_size, dtype=int)
    
    def matrix_to_binary_string(self, matrix: np.ndarray) -> List[str]:
        """Convert matrix to list of binary strings (MSB on left, LSB on right)"""
        result = []
        for row in matrix:
            binary_str = ''.join(str(bit) for bit in row)
            result.append(binary_str)
        return result
    
    def matrix_to_ulong_list(self, matrix: np.ndarray) -> List[int]:
        """Convert binary matrix to list of unsigned long integers"""
        result = []
        for row in matrix:
            binary_str = ''.join(str(bit) for bit in row)
            value = int(binary_str, 2)
            result.append(value)
        return result
    
    def print_matrix_structure(self, matrix: np.ndarray, matrix_name: str):
        """Print detailed matrix structure information"""
        binary_strings = self.matrix_to_binary_string(matrix)
        print()
        print(f"=== {matrix_name} Matrix Structure ===")
        print(f"Total size: {self.matrix_size} × {self.matrix_size}")
        
        if matrix_name == "DRAM":
            print(f"Bank function: 0-{self.num_bank_func-1}")
            print(f"Row address bits: {self.num_bank_func}-{self.num_bank_func + self.num_row_bits - 1} ({self.num_row_bits} bits)")
            print(f"Column address bits: {self.num_bank_func + self.num_row_bits}-{self.matrix_size-1} ({self.num_col_bits} bits)")
        
        print(f"\n{matrix_name}_MTX:")
        for i in range(self.matrix_size):
            binary_str = binary_strings[i]
            value = int(binary_str, 2)
            
            # Analyze the row pattern
            ones_count = binary_str.count('1')
            if ones_count == 1:
                bit_pos = binary_str.find('1')
                print(f"Row {i:2d}: {binary_str} = {value:10} (sets bit {29 - bit_pos})")
            else:
                ones_positions = []
                for bit_idx, bit in enumerate(binary_str):
                    if bit == '1':
                        bit_pos = 29 - bit_idx
                        ones_positions.append(bit_pos)
                print(f"Row {i:2d}: {binary_str} = {value:10} (sets bits {ones_positions})")

    def generate_config(self) -> dict:
        """Generate complete memory configuration"""
        # Calculate shift and mask values
        shift_mask_config = self.calculate_shift_mask()
        
        # Generate matrices
        dram_matrix = self.generate_dram_matrix()
        
        # Generate inverse matrix
        addr_matrix = self.generate_addr_matrix(dram_matrix)
        
        # Calculate identifier
        identifier = self.calculate_identifier()
        
        # Print matrix structures
        self.print_matrix_structure(dram_matrix, "DRAM")
        self.print_matrix_structure(addr_matrix, "ADDR")
        
        # Combine all configuration
        config = {
            "IDENTIFIER": identifier,
            "DRAM_MTX": self.matrix_to_ulong_list(dram_matrix),
            "ADDR_MTX": self.matrix_to_ulong_list(addr_matrix)
        }
        config.update(shift_mask_config)
        
        return {"MemConfiguration": config}
    
    def save_to_json(self, filename: str):
        """Save configuration to JSON file"""
        config = self.generate_config()
        
        with open(filename, 'w') as f:
            json.dump(config, f, indent=2)
        
        print(f"\nConfiguration file generated: {filename}")
        print(f"IDENTIFIER: {config['MemConfiguration']['IDENTIFIER']} (0x{config['MemConfiguration']['IDENTIFIER']:08X})")

if __name__ == "__main__":
    # Generate configuration using values from config.json
    generator = MemConfigGenerator("config.json")
    generator.save_to_json("../output/reverse_result/mem_config.json")